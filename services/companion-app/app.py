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
<title>Body ECU — Cloud Dashboard</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Red+Hat+Text:wght@400;500;700&family=Red+Hat+Mono:wght@400;500&display=swap" rel="stylesheet">
<style>
  :root {
    --rh-red: #EE0000;
    --gray-95: #151515;
    --gray-50: #707070;
    --gray-20: #E0E0E0;
    --gray-05: #F5F5F5;
    --blue-50: #0066CC;
    --blue-10: #E0F0FF;
    --green-50: #63993D;
    --green-20: #D1F1BB;
    --green-10: #E9F7DF;
    --red-orange: #F0561D;
    --yellow-40: #B98412;
    --purple-50: #5E40BE;
  }
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: 'Red Hat Text', -apple-system, BlinkMacSystemFont, sans-serif;
    background: var(--gray-05);
    color: var(--gray-95);
    min-height: 100vh;
  }

  /* ── top bar ── */
  .topbar {
    background: var(--gray-95);
    color: #fff;
    padding: 12px 24px;
    display: flex;
    align-items: center;
    justify-content: space-between;
  }
  .topbar-left { display: flex; align-items: center; gap: 14px; }
  .topbar h1 {
    font-size: 15px;
    font-weight: 700;
    letter-spacing: .01em;
  }
  .topbar h1 .sep { color: var(--gray-50); font-weight: 400; margin: 0 4px; }
  .topbar h1 .sub { font-weight: 400; color: var(--gray-20); }
  .rh-logo { height: 32px; width: auto; flex-shrink: 0; }
  .rh-logo .hat-shadow { fill: #fff; }
  .rh-logo .hat { fill: var(--rh-red); }
  .rh-logo .wordmark { fill: #fff; }
  .conn-tag {
    display: flex; align-items: center; gap: 7px;
    font-size: 12px; font-weight: 500; color: var(--gray-20);
  }
  .conn-dot {
    width: 7px; height: 7px; border-radius: 50%;
    background: var(--red-orange);
  }
  .conn-dot.ok { background: var(--green-50); }

  /* ── layout ── */
  .shell { max-width: 1160px; margin: 0 auto; padding: 20px 24px 32px; }

  .vin-bar {
    display: flex; align-items: center; gap: 10px;
    margin-bottom: 18px;
    font-size: 13px; color: var(--gray-50);
  }
  .vin-bar strong { color: var(--gray-95); font-weight: 500; }
  .vin-bar code {
    font-family: 'Red Hat Mono', monospace;
    font-size: 13px;
    color: var(--gray-95);
    background: #fff;
    border: 1px solid var(--gray-20);
    padding: 2px 8px;
    border-radius: 3px;
  }

  .grid {
    display: grid;
    grid-template-columns: repeat(12, 1fr);
    gap: 14px;
    margin-bottom: 14px;
  }
  .col-4 { grid-column: span 4; }
  .col-6 { grid-column: span 6; }
  .col-3 { grid-column: span 3; }
  @media (max-width: 900px) {
    .col-4, .col-6, .col-3 { grid-column: span 12; }
  }

  /* ── card ── */
  .card {
    background: #fff;
    border: 1px solid var(--gray-20);
    border-radius: 3px;
    padding: 16px 18px;
  }
  .card-head {
    font-size: 11px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: .06em;
    color: var(--gray-50);
    margin-bottom: 14px;
    padding-bottom: 8px;
    border-bottom: 1px solid var(--gray-20);
  }

  .kv { display: flex; justify-content: space-between; align-items: center; padding: 5px 0; }
  .kv-label { font-size: 13px; color: var(--gray-50); }
  .kv-val { font-size: 13px; font-weight: 500; }

  /* ── badges ── */
  .tag {
    display: inline-block;
    padding: 2px 9px;
    border-radius: 3px;
    font-size: 12px;
    font-weight: 700;
    letter-spacing: .02em;
  }
  .tag-ok      { background: var(--green-10); color: var(--green-50); border: 1px solid var(--green-20); }
  .tag-err     { background: #FDE8E8; color: var(--red-orange); border: 1px solid #FACACA; }
  .tag-warn    { background: #FFF4E0; color: var(--yellow-40); border: 1px solid #F5DDA0; }
  .tag-info    { background: var(--blue-10); color: var(--blue-50); border: 1px solid #B3D7FF; }
  .tag-neutral { background: var(--gray-05); color: var(--gray-50); border: 1px solid var(--gray-20); }

  /* ── speed display ── */
  .speed-wrap { text-align: center; padding: 8px 0 4px; }
  .speed-num {
    font-family: 'Red Hat Mono', monospace;
    font-size: 42px;
    font-weight: 500;
    line-height: 1;
    color: var(--gray-95);
  }
  .speed-unit { font-size: 12px; color: var(--gray-50); margin-top: 2px; }

  /* ── buttons ── */
  .btn-row { display: flex; gap: 8px; margin-top: 10px; }
  .btn {
    font-family: 'Red Hat Text', sans-serif;
    font-size: 12px;
    font-weight: 700;
    padding: 6px 16px;
    border-radius: 3px;
    border: none;
    cursor: pointer;
    transition: background .12s;
  }
  .btn:active { opacity: .8; }
  .btn-primary { background: var(--blue-50); color: #fff; }
  .btn-primary:hover { background: #004D99; }
  .btn-danger  { background: var(--red-orange); color: #fff; }
  .btn-danger:hover { background: #D44A18; }
  .btn-sm { padding: 3px 10px; font-size: 11px; }
  .btn-ghost {
    background: transparent;
    border: 1px solid var(--gray-20);
    color: var(--gray-95);
  }
  .btn-ghost:hover { background: var(--gray-05); border-color: var(--gray-50); }

  /* ── light row ── */
  .light-row {
    display: flex; align-items: center; justify-content: space-between;
    padding: 5px 0;
  }
  .light-row .left { display: flex; align-items: center; gap: 8px; }
  .light-row .left .kv-label { min-width: 80px; }
  .light-controls { display: flex; align-items: center; gap: 5px; }

  /* ── log panels ── */
  .panels { display: grid; grid-template-columns: 1fr 1fr; gap: 14px; }
  @media (max-width: 900px) { .panels { grid-template-columns: 1fr; } }
  .panel {
    background: #fff;
    border: 1px solid var(--gray-20);
    border-radius: 3px;
    padding: 16px 18px;
    display: flex; flex-direction: column;
  }
  .panel-head {
    font-size: 11px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: .06em;
    color: var(--gray-50);
    margin-bottom: 10px;
    padding-bottom: 8px;
    border-bottom: 1px solid var(--gray-20);
    display: flex; justify-content: space-between; align-items: center;
  }
  .log-scroll {
    flex: 1;
    max-height: 260px;
    overflow-y: auto;
    font-family: 'Red Hat Mono', monospace;
    font-size: 11px;
    line-height: 1.8;
    color: var(--gray-50);
  }
  .log-scroll div { border-bottom: 1px solid var(--gray-05); padding: 0 2px; }
  .log-ts { color: var(--gray-50); margin-right: 6px; opacity: .6; }

  /* ── toast ── */
  .toast {
    position: fixed; bottom: 20px; right: 20px;
    padding: 8px 16px; border-radius: 3px;
    font-size: 12px; font-weight: 500;
    opacity: 0; transition: opacity .25s;
    pointer-events: none; z-index: 10;
  }
  .toast.show { opacity: 1; }
  .toast-ok  { background: var(--green-50); color: #fff; }
  .toast-err { background: var(--red-orange); color: #fff; }
</style>
</head>
<body>

<div class="topbar">
  <div class="topbar-left">
    <svg class="rh-logo" viewBox="0 0 192 195" xmlns="http://www.w3.org/2000/svg"><path class="hat-shadow" d="M158.1,62.11a14,14,0,0,1,.31,3.42C158.41,80.41,140.3,83,127.8,83,79.15,83,42.86,52.76,42.86,43.54a6.12,6.12,0,0,1,.22-1.94l-3.67,9.06A18.45,18.45,0,0,0,37.9,58c0,18.11,41,45.48,87.74,45.48,20.7,0,36.43-7.76,36.43-21.77,0-1.08,0-1.94-1.72-10.13Z"/><path class="hat" d="M127.8,83c12.5,0,30.61-2.58,30.61-17.46a14,14,0,0,0-.31-3.42l-7.45-32.36c-1.73-7.12-3.23-10.35-15.74-16.6C125.21,8.19,104.08,0,97.83,0,92,0,90.29,7.54,83.39,7.54c-6.68,0-11.64-5.6-17.89-5.6-6,0-9.92,4.09-12.94,12.5,0,0-8.4,23.72-9.48,27.16a6.12,6.12,0,0,0-.22,1.94C42.86,52.76,79.15,83,127.8,83m32.55-11.42c1.72,8.19,1.72,9.05,1.72,10.13,0,14-15.73,21.77-36.43,21.77C78.86,103.47,37.9,76.1,37.9,58a18.45,18.45,0,0,1,1.51-7.33C22.6,51.52.83,54.54.83,73.72.83,105.2,75.41,144,134.48,144c45.27,0,56.69-20.48,56.69-36.65,0-12.72-11-27.16-30.82-35.78"/><path class="wordmark" d="M165.67,186.2c0,5.15,3.11,7.66,8.77,7.66a22.6,22.6,0,0,0,5.15-.73v-6a10.58,10.58,0,0,1-3.33.51c-2.33,0-3.19-.73-3.19-2.92v-9.18h6.75V169.4h-6.75v-7.8l-7.4,1.59v6.21h-4.88v6.16h4.88Zm-23,.13c0-1.59,1.6-2.37,4-2.37a18.6,18.6,0,0,1,4.38.55v3.1a9.3,9.3,0,0,1-4.61,1.14c-2.38,0-3.79-.91-3.79-2.42M145,194a11.24,11.24,0,0,0,6.66-1.87v1.46h7.3V178.07c0-5.88-4-9.13-10.59-9.13a26.9,26.9,0,0,0-11.27,2.65l2.65,5.43a19.82,19.82,0,0,1,7.3-1.91c3.06,0,4.61,1.18,4.61,3.6v1.19a21.32,21.32,0,0,0-5.48-.69c-6.2,0-9.95,2.6-9.95,7.26,0,4.24,3.38,7.48,8.77,7.48m-40.12-.41h7.85V181h13.14v12.5h7.85V161.6h-7.85v12.27H112.69V161.6h-7.85ZM74.93,181.45a6.12,6.12,0,0,1,6.34-6.12,7.45,7.45,0,0,1,5.11,1.88v8.44a7.08,7.08,0,0,1-5.11,1.92,6.14,6.14,0,0,1-6.34-6.12m11.54,12.09h7.31V160l-7.4,1.6v9.08A12.44,12.44,0,1,0,80,193.86a10.86,10.86,0,0,0,6.48-2.1ZM53,175A5.34,5.34,0,0,1,58,178.85H48A5,5,0,0,1,53,175M40.52,181.5c0,7,5.75,12.5,13.14,12.5a14.53,14.53,0,0,0,10.09-3.65L58.86,186A6.51,6.51,0,0,1,54,187.84,6.26,6.26,0,0,1,48.09,184H65.3v-1.83c0-7.67-5.16-13.19-12.19-13.19A12.4,12.4,0,0,0,40.52,181.5M27.79,168.31c2.6,0,4.06,1.64,4.06,3.6s-1.46,3.61-4.06,3.61H20v-7.21ZM12.18,193.54H20V181.91h6l6,11.63h8.76l-7-12.77a9.68,9.68,0,0,0,6-9c0-5.75-4.52-10.17-11.27-10.17H12.18Z"/></svg>
    <h1>Body ECU<span class="sep">/</span><span class="sub">Cloud Dashboard</span></h1>
  </div>
  <div class="conn-tag">
    <span id="nats-dot" class="conn-dot"></span>
    <span id="nats-label">Connecting</span>
  </div>
</div>

<div class="shell">

  <div class="vin-bar">
    <strong>VIN</strong>
    <code id="vin-val">&mdash;</code>
  </div>

  <div class="grid">

    <div class="card col-3">
      <div class="card-head">Ignition</div>
      <div class="kv">
        <span class="kv-label">Mode</span>
        <span id="mode-badge" class="tag tag-neutral">Unknown</span>
      </div>
    </div>

    <div class="card col-3">
      <div class="card-head">Speed</div>
      <div class="speed-wrap">
        <div class="speed-num" id="speed-val">&mdash;</div>
        <div class="speed-unit">km/h</div>
      </div>
    </div>

    <div class="card col-3">
      <div class="card-head">Door</div>
      <div class="kv">
        <span class="kv-label">State</span>
        <span id="door-badge" class="tag tag-neutral">Unknown</span>
      </div>
      <div class="kv">
        <span class="kv-label">Response</span>
        <span id="cmd-resp" class="kv-val" style="color:var(--gray-50)">&mdash;</span>
      </div>
      <div class="btn-row">
        <button class="btn btn-danger btn-sm" onclick="cmd('/api/door/lock')">Lock</button>
        <button class="btn btn-primary btn-sm" onclick="cmd('/api/door/unlock')">Unlock</button>
      </div>
    </div>

    <div class="card col-3">
      <div class="card-head">Lighting</div>
      <div class="light-row">
        <div class="left"><span class="kv-label">Headlights</span><span id="light-head" class="tag tag-neutral">&mdash;</span></div>
        <div class="light-controls">
          <button class="btn btn-ghost btn-sm" onclick="lightCmd(0,'on')">On</button>
          <button class="btn btn-ghost btn-sm" onclick="lightCmd(0,'off')">Off</button>
        </div>
      </div>
      <div class="light-row">
        <div class="left"><span class="kv-label">Turn</span><span id="light-turn" class="tag tag-neutral">&mdash;</span></div>
        <div class="light-controls">
          <button class="btn btn-ghost btn-sm" onclick="lightCmd(1,'on')">On</button>
          <button class="btn btn-ghost btn-sm" onclick="lightCmd(1,'off')">Off</button>
        </div>
      </div>
      <div class="light-row">
        <div class="left"><span class="kv-label">Brake</span><span id="light-brake" class="tag tag-neutral">&mdash;</span></div>
        <div class="light-controls">
          <button class="btn btn-ghost btn-sm" onclick="lightCmd(2,'on')">On</button>
          <button class="btn btn-ghost btn-sm" onclick="lightCmd(2,'off')">Off</button>
        </div>
      </div>
    </div>

  </div>

  <div class="panels">
    <div class="panel">
      <div class="panel-head">
        Events
        <button class="btn btn-ghost btn-sm" onclick="clearAll()">Clear</button>
      </div>
      <div id="ev-log" class="log-scroll"><div>Waiting for events</div></div>
    </div>
    <div class="panel">
      <div class="panel-head">Protocol log</div>
      <div id="nats-log" class="log-scroll"><div>Waiting</div></div>
    </div>
  </div>

</div>

<div id="toast" class="toast"></div>

<script>
const $ = s => document.querySelector(s);
const MODE = {0:'OFF',1:'ACC',2:'RUN',3:'CRANK'};
const MODE_TAG = {0:'tag-neutral',1:'tag-warn',2:'tag-ok',3:'tag-warn'};

function toast(m,ok){
  const t=$('#toast');
  t.textContent=m;
  t.className='toast show '+(ok?'toast-ok':'toast-err');
  setTimeout(()=>t.className='toast',2200);
}

async function cmd(url){
  try{
    const r=await fetch(url,{method:'POST'});
    if(!r.ok) throw new Error(r.statusText);
    const d=await r.json();
    toast(d.command,true);
    setTimeout(poll,300);
  }catch(e){toast('Failed: '+e.message,false);}
}

async function lightCmd(id,action){ await cmd('/api/light/'+id+'/'+action); }
async function clearAll(){ await fetch('/api/events',{method:'DELETE'}); poll(); }

function updDoor(s){
  const b=$('#door-badge');
  if(s.door_locked==null){ b.textContent='Unknown'; b.className='tag tag-neutral'; }
  else if(s.door_locked){ b.textContent='Locked'; b.className='tag tag-err'; }
  else{ b.textContent='Unlocked'; b.className='tag tag-ok'; }

  const r=$('#cmd-resp');
  if(s.last_command_response==null){ r.textContent='\u2014'; r.style.color='var(--gray-50)'; }
  else if(s.last_command_response===0){ r.textContent='OK'; r.style.color='var(--green-50)'; }
  else{ r.textContent='0x'+s.last_command_response.toString(16).toUpperCase(); r.style.color='var(--red-orange)'; }
}

function updMode(s){
  const b=$('#mode-badge');
  if(s.vehicle_mode==null){ b.textContent='Unknown'; b.className='tag tag-neutral'; }
  else{
    b.textContent=MODE[s.vehicle_mode]||('0x'+s.vehicle_mode.toString(16));
    b.className='tag '+(MODE_TAG[s.vehicle_mode]||'tag-info');
  }
}

function updSpeed(s){
  $('#speed-val').textContent=(s.speed_kmh!=null)?s.speed_kmh.toFixed(0):'\u2014';
}

function updLights(s){
  const st=s.light_status;
  if(st==null){
    ['#light-head','#light-turn','#light-brake'].forEach(id=>{
      const b=$(id); b.textContent='\u2014'; b.className='tag tag-neutral';
    });
    return;
  }
  function lb(id,bit){
    const b=$(id); const on=(st>>bit)&1;
    b.textContent=on?'ON':'OFF';
    b.className='tag '+(on?'tag-ok':'tag-neutral');
  }
  lb('#light-head',0); lb('#light-turn',1); lb('#light-brake',2);
}

function updEvents(evts){
  const el=$('#ev-log');
  if(!evts.length){ el.innerHTML='<div>No events yet.</div>'; return; }
  el.innerHTML=evts.slice().reverse().map(e=>{
    let h='<span class="log-ts">'+e.ts+'</span>';
    if(e.type==='door_state')
      h+='Door '+(e.locked?'<span class="tag tag-err" style="font-size:10px">LOCKED</span>':'<span class="tag tag-ok" style="font-size:10px">UNLOCKED</span>');
    else if(e.type==='command_response')
      h+='Cmd '+(e.code===0?'<span class="tag tag-ok" style="font-size:10px">OK</span>':'<span class="tag tag-err" style="font-size:10px">FAIL</span>');
    else if(e.type==='mode')
      h+='Mode <span class="tag '+(MODE_TAG[e.mode]||'tag-info')+'" style="font-size:10px">'+e.name+'</span>';
    else if(e.type==='speed')
      h+='Speed '+e.kmh+' km/h';
    else if(e.type==='lights')
      h+='Lights 0x'+e.status.toString(16).padStart(2,'0');
    else h+=JSON.stringify(e);
    return '<div>'+h+'</div>';
  }).join('');
}

function updLogs(logs){
  const el=$('#nats-log');
  if(!logs.length){ el.innerHTML='<div>Waiting</div>'; return; }
  el.innerHTML=logs.slice().reverse().map(l=>
    '<div><span class="log-ts">'+l.ts+'</span>'+l.msg+'</div>'
  ).join('');
}

async function poll(){
  try{
    const [sr,er,hr,lr]=await Promise.all([
      fetch('/api/state'),fetch('/api/events?limit=50'),
      fetch('/health'),fetch('/api/logs?limit=80')
    ]);
    const s=await sr.json(), ev=await er.json(),
          h=await hr.json(), lo=await lr.json();

    $('#vin-val').textContent=s.vin||'\u2014';
    updDoor(s); updMode(s); updSpeed(s); updLights(s);
    updEvents(ev.events); updLogs(lo.logs);

    const d=$('#nats-dot'), l=$('#nats-label');
    if(h.healthy){ d.classList.add('ok'); l.textContent='Connected'; }
    else{ d.classList.remove('ok'); l.textContent='Disconnected'; }
  }catch(e){}
}

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
