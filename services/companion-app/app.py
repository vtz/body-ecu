"""Companion App — Body ECU Cloud Dashboard.

Connects to the vehicle's NATS broker and provides a web UI + REST API
to monitor and control all body ECU services.

Usage:
    python app.py [--nats-url nats://localhost:4222] [--port 5002]
"""

import argparse
import asyncio
import json
import logging
import struct
import sys
import threading
import time as _time
from dataclasses import dataclass, field
from datetime import datetime
from typing import Optional

import nats
from flask import Flask, jsonify, request as flask_request, render_template_string

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
)
logger = logging.getLogger("companion-app")

DEFAULT_VIN = "WVWZZZ3CZWE000001"
MODE_NAMES = {0: "OFF", 1: "ACC", 2: "RUN", 3: "CRANK"}
LIGHT_NAMES = {0: "Headlights", 1: "Turn Signal", 2: "Brake Light"}


@dataclass
class VehicleState:
    vin: str = DEFAULT_VIN
    door_locked: Optional[bool] = None
    last_command_response: Optional[int] = None
    vehicle_mode: Optional[int] = None
    speed_kmh: Optional[float] = None
    light_status: Optional[int] = None
    events: list = field(default_factory=list)
    logs: list = field(default_factory=list)

    def add_event(self, event: dict):
        event["ts"] = datetime.now().strftime("%H:%M:%S")
        self.events.append(event)
        if len(self.events) > 200:
            self.events = self.events[-200:]

    def add_log(self, msg: str):
        entry = {"ts": datetime.now().strftime("%H:%M:%S.%f")[:-3], "msg": msg}
        self.logs.append(entry)
        if len(self.logs) > 300:
            self.logs = self.logs[-300:]


class NatsClient:
    def __init__(self, nats_url: str, state: VehicleState):
        self._url = nats_url
        self._state = state
        self._nc: Optional[nats.NATS] = None
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._thread: Optional[threading.Thread] = None

    def start(self):
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        self._loop.run_until_complete(self._connect_and_listen())

    async def _connect_and_listen(self):
        self._nc = await nats.connect(self._url)
        self._state.add_log(f"Connected to NATS at {self._url}")
        logger.info("Connected to NATS at %s", self._url)

        subs = {
            "vehicles.*.info.vin": self._on_vin,
            "vehicles.*.state.door.locked": self._on_lock_state,
            "vehicles.*.command.door.response": self._on_command_response,
            "vehicles.*.state.vehicle.mode": self._on_vehicle_mode,
            "vehicles.*.state.vehicle.speed": self._on_vehicle_speed,
            "vehicles.*.state.lights.status": self._on_light_status,
            "vehicles.*.command.lights.set": self._on_light_command_echo,
        }
        for subject, cb in subs.items():
            await self._nc.subscribe(subject, cb=cb)
            self._state.add_log(f"SUB {subject}")

        while self._nc.is_connected:
            await asyncio.sleep(0.1)

    def _learn_vin_from_subject(self, subject: str):
        """Extract VIN from subject like 'vehicles.<VIN>.state.door.locked'."""
        parts = subject.split(".")
        if len(parts) >= 2:
            vin = parts[1]
            if vin != "*" and vin != self._state.vin:
                old = self._state.vin
                self._state.vin = vin
                self._state.add_log(f"VIN → {vin} (was {old})")
                logger.info("Learned VIN from NATS: %s", vin)

    async def _on_vin(self, msg):
        if msg.data:
            vin = msg.data.decode("utf-8", errors="replace").rstrip("\x00")
            if vin and vin != self._state.vin:
                old_vin = self._state.vin
                self._state.vin = vin
                self._state.add_event({"type": "vin", "vin": vin})
                self._state.add_log(f"VIN → {vin} (was {old_vin})")
                logger.info("VIN received from MCU: %s", vin)

    async def _on_lock_state(self, msg):
        self._learn_vin_from_subject(msg.subject)
        if msg.data:
            locked = msg.data[0] != 0
            self._state.door_locked = locked
            label = "LOCKED" if locked else "UNLOCKED"
            self._state.add_event({"type": "door_state", "locked": locked})
            self._state.add_log(f"Door → {label}")
            logger.info("Door state: %s", label)

    async def _on_command_response(self, msg):
        self._learn_vin_from_subject(msg.subject)
        if msg.data:
            code = msg.data[0]
            self._state.last_command_response = code
            self._state.add_event({"type": "command_response", "code": code})
            self._state.add_log(f"Cmd response: 0x{code:02X}")

    async def _on_vehicle_mode(self, msg):
        self._learn_vin_from_subject(msg.subject)
        if msg.data:
            mode = msg.data[0]
            self._state.vehicle_mode = mode
            name = MODE_NAMES.get(mode, f"0x{mode:02X}")
            self._state.add_event({"type": "mode", "mode": mode, "name": name})
            self._state.add_log(f"Mode → {name}")
            logger.info("Vehicle mode: %s", name)

    async def _on_vehicle_speed(self, msg):
        self._learn_vin_from_subject(msg.subject)
        if msg.data and len(msg.data) >= 4:
            speed = struct.unpack(">f", msg.data[:4])[0]
            self._state.speed_kmh = round(speed, 1)
            self._state.add_event({"type": "speed", "kmh": self._state.speed_kmh})
            self._state.add_log(f"Speed → {self._state.speed_kmh} km/h")

    async def _on_light_command_echo(self, msg):
        pass

    async def _on_light_status(self, msg):
        self._learn_vin_from_subject(msg.subject)
        if msg.data:
            status = msg.data[0]
            self._state.light_status = status
            self._state.add_event({"type": "lights", "status": status})
            self._state.add_log(f"Lights → 0x{status:02X}")
            logger.info("Light status: 0x%02X", status)

    def publish_sync(self, subject: str, data: bytes):
        if not self._nc or not self._loop:
            raise RuntimeError("NATS client not connected")
        future = asyncio.run_coroutine_threadsafe(
            self._nc.publish(subject, data), self._loop
        )
        future.result(timeout=5.0)
        self._state.add_log(f"PUB {subject} ({len(data)}B)")


def create_app(nats_url: str) -> Flask:
    app = Flask(__name__)
    state = VehicleState()
    nats_client = NatsClient(nats_url, state)

    @app.before_request
    def ensure_nats():
        if nats_client._nc is None:
            nats_client.start()
            for _ in range(50):
                if nats_client._nc is not None:
                    break
                _time.sleep(0.1)

    @app.route("/api/door/lock", methods=["POST"])
    def lock_door():
        subject = f"vehicles.{state.vin}.command.door.lock"
        nats_client.publish_sync(subject, bytes([0x01]))
        return jsonify({"command": "lock", "sent": True})

    @app.route("/api/door/unlock", methods=["POST"])
    def unlock_door():
        subject = f"vehicles.{state.vin}.command.door.lock"
        nats_client.publish_sync(subject, bytes([0x00]))
        return jsonify({"command": "unlock", "sent": True})

    @app.route("/api/light/<int:light_id>/<action>", methods=["POST"])
    def set_light(light_id, action):
        if light_id not in (0, 1, 2):
            return jsonify({"error": "light_id must be 0, 1, or 2"}), 400
        on = 1 if action == "on" else 0
        subject = f"vehicles.{state.vin}.command.lights.set"
        nats_client.publish_sync(subject, bytes([light_id, on]))
        name = LIGHT_NAMES.get(light_id, f"Light {light_id}")
        return jsonify({"command": f"{name} {'ON' if on else 'OFF'}", "sent": True})

    @app.route("/api/state", methods=["GET"])
    def get_state():
        return jsonify({
            "vin": state.vin,
            "door_locked": state.door_locked,
            "last_command_response": state.last_command_response,
            "vehicle_mode": state.vehicle_mode,
            "vehicle_mode_name": MODE_NAMES.get(state.vehicle_mode, "Unknown")
                if state.vehicle_mode is not None else None,
            "speed_kmh": state.speed_kmh,
            "light_status": state.light_status,
        })

    @app.route("/api/events", methods=["GET"])
    def get_events():
        limit = flask_request.args.get("limit", 50, type=int)
        return jsonify({"events": state.events[-limit:]})

    @app.route("/api/events", methods=["DELETE"])
    def clear_events():
        state.events.clear()
        state.logs.clear()
        state.door_locked = None
        state.last_command_response = None
        state.vehicle_mode = None
        state.speed_kmh = None
        state.light_status = None
        return jsonify({"cleared": True})

    @app.route("/api/logs", methods=["GET"])
    def get_logs():
        limit = flask_request.args.get("limit", 80, type=int)
        return jsonify({"logs": state.logs[-limit:]})

    @app.route("/health", methods=["GET"])
    def health():
        connected = nats_client._nc is not None and nats_client._nc.is_connected
        return jsonify({"healthy": connected, "nats_url": nats_url})

    @app.route("/")
    def dashboard():
        return render_template_string(DASHBOARD_HTML)

    return app


DASHBOARD_HTML = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Body ECU — Dashboard</title>
<style>
  :root { --bg: #0f172a; --card: #1e293b; --accent: #3b82f6; --ok: #22c55e;
          --warn: #f59e0b; --danger: #ef4444; --txt: #e2e8f0; --muted: #94a3b8;
          --border: #334155; }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: system-ui, -apple-system, sans-serif; background: var(--bg);
         color: var(--txt); min-height: 100vh; padding: 1.5rem 2rem; }
  header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 1.5rem; }
  h1 { font-size: 1.4rem; }
  h1 span { color: var(--muted); font-weight: 400; font-size: 0.85rem; }
  .nats-pill { display: inline-flex; align-items: center; gap: 6px; padding: 4px 12px;
               border-radius: 20px; font-size: 0.78rem; font-weight: 500;
               background: var(--card); border: 1px solid var(--border); }
  .dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block; }
  .dot-g { background: var(--ok); box-shadow: 0 0 6px var(--ok); }
  .dot-r { background: var(--danger); }

  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(260px,1fr));
          gap: 1rem; margin-bottom: 1rem; }
  .card { background: var(--card); border-radius: 12px; padding: 1.2rem;
          border: 1px solid var(--border); }
  .card h2 { font-size: 0.75rem; text-transform: uppercase; letter-spacing: .08em;
             color: var(--muted); margin-bottom: .9rem; display: flex;
             align-items: center; gap: 6px; }
  .card h2 .icon { font-size: 1rem; }
  .metric { display: flex; justify-content: space-between; align-items: center; margin-bottom: .6rem; }
  .metric-label { color: var(--muted); font-size: 0.85rem; }
  .metric-val { font-weight: 600; }
  .badge { padding: 3px 10px; border-radius: 6px; font-size: 0.78rem; font-weight: 600; }
  .b-green { background: rgba(34,197,94,.15); color: var(--ok); }
  .b-red { background: rgba(239,68,68,.15); color: var(--danger); }
  .b-yellow { background: rgba(245,158,11,.15); color: var(--warn); }
  .b-blue { background: rgba(59,130,246,.15); color: var(--accent); }
  .b-muted { background: rgba(148,163,184,.15); color: var(--muted); }

  .speed-big { font-size: 2.8rem; font-weight: 700; text-align: center; line-height: 1; }
  .speed-unit { font-size: 0.85rem; color: var(--muted); font-weight: 400; }

  .btn-row { display: flex; gap: .5rem; flex-wrap: wrap; margin-top: .8rem; }
  .btn { border: none; border-radius: 8px; padding: .55rem 1.1rem; font-size: .82rem;
         font-weight: 600; cursor: pointer; transition: all .15s; }
  .btn:hover { opacity: .85; } .btn:active { transform: scale(.97); }
  .btn-lock { background: var(--danger); color: #fff; }
  .btn-unlock { background: var(--ok); color: #fff; }
  .btn-sm { padding: .35rem .7rem; font-size: .78rem; }
  .btn-outline { background: transparent; border: 1px solid var(--border); color: var(--txt); }

  .panels { display: grid; grid-template-columns: 1fr 1fr; gap: 1rem; }
  @media (max-width: 800px) { .panels { grid-template-columns: 1fr; } }
  .panel { background: var(--card); border-radius: 12px; padding: 1.2rem;
           border: 1px solid var(--border); }
  .panel h2 { font-size: .75rem; text-transform: uppercase; letter-spacing: .08em;
              color: var(--muted); margin-bottom: .6rem; display: flex;
              justify-content: space-between; align-items: center; }
  .log-box { max-height: 280px; overflow-y: auto; font-family: 'SF Mono', 'Fira Code', monospace;
             font-size: .75rem; line-height: 1.7; color: var(--muted); }
  .log-box div { padding: 1px 0; border-bottom: 1px solid rgba(148,163,184,.07); }
  .log-ts { color: #475569; margin-right: .4rem; }
  .ev-badge { margin-right: .3rem; }

  .toast { position: fixed; bottom: 1.5rem; right: 1.5rem; padding: .65rem 1.1rem;
           border-radius: 8px; font-size: .82rem; font-weight: 500; opacity: 0;
           transition: opacity .3s; pointer-events: none; z-index: 10; }
  .toast.show { opacity: 1; }
  .toast-ok { background: var(--ok); color: #fff; }
  .toast-err { background: var(--danger); color: #fff; }
</style>
</head>
<body>

<header>
  <h1>Body ECU <span>— Dashboard</span></h1>
  <div class="nats-pill">
    <span id="nats-dot" class="dot dot-r"></span>
    <span id="nats-label">Connecting…</span>
  </div>
</header>

<div class="grid">

  <div class="card">
    <h2><span class="icon">🚗</span> Vehicle Mode</h2>
    <div class="metric">
      <span class="metric-label">Ignition</span>
      <span id="mode-badge" class="badge b-muted">Unknown</span>
    </div>
  </div>

  <div class="card">
    <h2><span class="icon">🆔</span> Vehicle</h2>
    <div class="metric">
      <span class="metric-label">VIN</span>
      <span id="vin-val" class="metric-val" style="font-size:0.82rem;font-family:'SF Mono','Fira Code',monospace">—</span>
    </div>
  </div>

  <div class="card">
    <h2><span class="icon">🔒</span> Door Lock</h2>
    <div class="metric">
      <span class="metric-label">State</span>
      <span id="door-badge" class="badge b-muted">Unknown</span>
    </div>
    <div class="metric">
      <span class="metric-label">Last Response</span>
      <span id="cmd-resp" class="metric-val" style="color:var(--muted)">—</span>
    </div>
    <div class="btn-row">
      <button class="btn btn-lock" onclick="cmd('/api/door/lock')">Lock</button>
      <button class="btn btn-unlock" onclick="cmd('/api/door/unlock')">Unlock</button>
    </div>
  </div>

  <div class="card" style="display:flex;flex-direction:column;align-items:center;justify-content:center">
    <h2 style="align-self:flex-start"><span class="icon">⚡</span> Speed</h2>
    <div class="speed-big"><span id="speed-val">—</span></div>
    <div class="speed-unit">km/h</div>
  </div>

  <div class="card">
    <h2><span class="icon">💡</span> Lighting</h2>
    <div class="metric">
      <span class="metric-label">Headlights</span>
      <span style="display:flex;align-items:center;gap:6px">
        <span id="light-head" class="badge b-muted">—</span>
        <button class="btn btn-sm btn-outline" onclick="lightCmd(0,'on')">ON</button>
        <button class="btn btn-sm btn-outline" onclick="lightCmd(0,'off')">OFF</button>
      </span>
    </div>
    <div class="metric">
      <span class="metric-label">Turn Signal</span>
      <span style="display:flex;align-items:center;gap:6px">
        <span id="light-turn" class="badge b-muted">—</span>
        <button class="btn btn-sm btn-outline" onclick="lightCmd(1,'on')">ON</button>
        <button class="btn btn-sm btn-outline" onclick="lightCmd(1,'off')">OFF</button>
      </span>
    </div>
    <div class="metric">
      <span class="metric-label">Brake Light</span>
      <span style="display:flex;align-items:center;gap:6px">
        <span id="light-brake" class="badge b-muted">—</span>
        <button class="btn btn-sm btn-outline" onclick="lightCmd(2,'on')">ON</button>
        <button class="btn btn-sm btn-outline" onclick="lightCmd(2,'off')">OFF</button>
      </span>
    </div>
  </div>

</div>

<div class="panels">
  <div class="panel">
    <h2>Events <button class="btn btn-sm btn-outline" onclick="clearAll()">Clear</button></h2>
    <div id="ev-log" class="log-box"><div>Waiting for events…</div></div>
  </div>
  <div class="panel">
    <h2>NATS Log</h2>
    <div id="nats-log" class="log-box"><div>Waiting…</div></div>
  </div>
</div>

<div id="toast" class="toast"></div>

<script>
const $ = s => document.querySelector(s);
const MODE = {0:'OFF',1:'ACC',2:'RUN',3:'CRANK'};
const MODE_CLS = {0:'b-muted',1:'b-yellow',2:'b-green',3:'b-yellow'};

function toast(m,ok){const t=$('#toast');t.textContent=m;
  t.className='toast show '+(ok?'toast-ok':'toast-err');setTimeout(()=>t.className='toast',2000);}

async function cmd(url){
  try{const r=await fetch(url,{method:'POST'});if(!r.ok)throw new Error(r.statusText);
    const d=await r.json();toast('Sent: '+d.command,true);setTimeout(poll,300);
  }catch(e){toast('Failed: '+e.message,false);}}

async function lightCmd(id,action){
  await cmd('/api/light/'+id+'/'+action);}

async function clearAll(){await fetch('/api/events',{method:'DELETE'});poll();}

function updDoor(s){
  const b=$('#door-badge');
  if(s.door_locked===null||s.door_locked===undefined){b.textContent='Unknown';b.className='badge b-muted';}
  else if(s.door_locked){b.textContent='Locked';b.className='badge b-red';}
  else{b.textContent='Unlocked';b.className='badge b-green';}
  const r=$('#cmd-resp');
  if(s.last_command_response===null||s.last_command_response===undefined){r.textContent='—';r.style.color='var(--muted)';}
  else if(s.last_command_response===0){r.textContent='✓ OK';r.style.color='var(--ok)';}
  else{r.textContent='✗ 0x'+s.last_command_response.toString(16);r.style.color='var(--danger)';}
}

function updMode(s){
  const b=$('#mode-badge');
  if(s.vehicle_mode===null||s.vehicle_mode===undefined){b.textContent='Unknown';b.className='badge b-muted';}
  else{b.textContent=MODE[s.vehicle_mode]||('0x'+s.vehicle_mode.toString(16));
       b.className='badge '+(MODE_CLS[s.vehicle_mode]||'b-blue');}
}

function updSpeed(s){
  const v=$('#speed-val');
  v.textContent=(s.speed_kmh!==null&&s.speed_kmh!==undefined)?s.speed_kmh.toFixed(0):'—';
}

function updLights(s){
  const st=s.light_status;
  if(st===null||st===undefined){
    ['#light-head','#light-turn','#light-brake'].forEach(id=>{const b=$(id);b.textContent='—';b.className='badge b-muted';});
    return;}
  function lb(id,bit){const b=$(id);const on=(st>>bit)&1;
    b.textContent=on?'ON':'OFF';b.className='badge '+(on?'b-green':'b-muted');}
  lb('#light-head',0);lb('#light-turn',1);lb('#light-brake',2);
}

function updEvents(evts){
  const el=$('#ev-log');
  if(!evts.length){el.innerHTML='<div>No events yet.</div>';return;}
  el.innerHTML=evts.slice().reverse().map(e=>{
    let html='<span class="log-ts">'+e.ts+'</span> ';
    if(e.type==='door_state') html+='<span class="ev-badge badge '+(e.locked?'b-red':'b-green')+'">'+(e.locked?'LOCKED':'UNLOCKED')+'</span>';
    else if(e.type==='command_response') html+='Cmd <span class="ev-badge badge '+(e.code===0?'b-green':'b-red')+'">'+(e.code===0?'OK':'FAIL')+'</span>';
    else if(e.type==='mode') html+='Mode → <span class="ev-badge badge '+(MODE_CLS[e.mode]||'b-blue')+'">'+e.name+'</span>';
    else if(e.type==='speed') html+='Speed → <b>'+e.kmh+'</b> km/h';
    else if(e.type==='lights') html+='Lights → <span class="badge b-blue">0x'+e.status.toString(16).padStart(2,'0')+'</span>';
    else html+=JSON.stringify(e);
    return '<div>'+html+'</div>';}).join('');
}

function updLogs(logs){
  const el=$('#nats-log');
  if(!logs.length){el.innerHTML='<div>Waiting…</div>';return;}
  el.innerHTML=logs.slice().reverse().map(l=>
    '<div><span class="log-ts">'+l.ts+'</span>'+l.msg+'</div>').join('');
}

async function poll(){
  try{
    const [sr,er,hr,lr]=await Promise.all([
      fetch('/api/state'),fetch('/api/events?limit=50'),fetch('/health'),fetch('/api/logs?limit=80')]);
    const s=await sr.json(),ev=await er.json(),h=await hr.json(),lo=await lr.json();
    $('#vin-val').textContent=s.vin||'—';
    updDoor(s);updMode(s);updSpeed(s);updLights(s);updEvents(ev.events);updLogs(lo.logs);
    const d=$('#nats-dot'),l=$('#nats-label');
    if(h.healthy){d.className='dot dot-g';l.textContent='Connected';}
    else{d.className='dot dot-r';l.textContent='Disconnected';}
  }catch(e){}}

setInterval(poll,1200);
poll();
</script>
</body>
</html>
"""


def main():
    parser = argparse.ArgumentParser(description="Body ECU Companion App")
    parser.add_argument("--nats-url", default="nats://localhost:4222")
    parser.add_argument("--port", type=int, default=5002)
    parser.add_argument("--host", default="127.0.0.1")
    args = parser.parse_args()

    app = create_app(args.nats_url)

    logger.info("Starting companion app on %s:%d", args.host, args.port)
    logger.info("NATS: %s, default VIN: %s", args.nats_url, DEFAULT_VIN)
    app.run(host=args.host, port=args.port, debug=False)


if __name__ == "__main__":
    main()
