# Quick Start Guide — Body ECU E2E Demo

This guide walks through setting up the full end-to-end demo: MCU hardware, MPU in a QEMU VM, and a web dashboard on your Mac.

## Prerequisites

| Component | Requirement |
|-----------|-------------|
| **NUCLEO board** | NUCLEO-H755ZI-Q connected via USB + Ethernet |
| **Zephyr SDK** | Installed (`~/.zephyrrc` sourced) |
| **QEMU VM** | AutoSD nightly image running (aarch64), SSH on port 2225 |
| **Mac tools** | Python 3.10+, Docker (Colima), `west`, `pip` |

### Network Setup

The NUCLEO board and QEMU VM must be on the same Ethernet segment for SOME/IP.
A typical configuration:

| Node | IP Address | Notes |
|------|-----------|-------|
| MCU (NUCLEO) | `192.168.100.10` | Static, set in Zephyr overlay |
| VM (QEMU) | `192.168.100.1` or DHCP | Must reach MCU on port 30490/UDP |
| Mac (host) | `127.0.0.1` | Accesses VM via SSH on port 2225 |

---

## Step 1 — Build and Flash the MCU Firmware

```bash
cd /path/to/body-ecu

# Initialize workspace (first time only)
west init -l .
west update

# Build for the NUCLEO board
export ZEPHYR_BASE=/path/to/zephyr
west build -b nucleo_h755zi_q/stm32h755xx/m7 app -d build/nucleo

# Flash
west flash -d build/nucleo
```

The MCU boots automatically and starts offering SOME/IP services on `0.0.0.0:30490`.

**Artifacts:**
- `build/nucleo/zephyr/zephyr.bin`
- `build/nucleo/zephyr/zephyr.hex`
- `build/nucleo/zephyr/zephyr.elf`

---

## Step 2 — Build the HPC RPM

```bash
# Ensure Docker is running (Colima on macOS)
colima start --memory 4 --cpu 4

# Build the RPM
bash packaging/build-rpm.sh hpc aarch64
```

This produces: `build/rpm/body-ecu-hpc-0.2.0-1.fc42.aarch64.rpm`

---

## Step 3 — Set Up the QEMU VM

### 3.1 — Copy files to the VM

```bash
# Copy RPM
scp -P 2225 build/rpm/body-ecu-hpc-0.2.0-1.fc42.aarch64.rpm root@127.0.0.1:/tmp/

# Copy VSS overlay for Kuksa
scp -P 2225 config/vss_overlay.json root@127.0.0.1:/etc/body-ecu/vss_overlay.json
```

### 3.2 — SSH into the VM and install

```bash
ssh -p 2225 root@127.0.0.1

# Install the RPM (use --force to upgrade)
dnf install -y /tmp/body-ecu-hpc-0.2.0-1.fc42.aarch64.rpm
```

### 3.3 — Start NATS server

```bash
# If not already installed:
# curl -L https://github.com/nats-io/nats-server/releases/download/v2.10.24/nats-server-v2.10.24-linux-arm64.tar.gz | tar xz
# cp nats-server-*/nats-server /usr/local/bin/

nats-server -js &
```

### 3.4 — Start Kuksa Databroker (with VSS overlay)

```bash
mkdir -p /etc/body-ecu

podman run -d --name kuksa \
  -p 55555:55555 \
  -v /etc/body-ecu/vss_overlay.json:/overlay.json:Z \
  ghcr.io/eclipse-kuksa/kuksa-databroker:0.6.0 \
  --vss vss_release_5.1.json,/overlay.json

# Verify it started
podman logs kuksa 2>&1 | tail -5
```

You should see `Populating metadata from file '/overlay.json'` without errors.

### 3.5 — Start the HPC application

```bash
KUKSA_HOST=127.0.0.1 KUKSA_PORT=55555 NATS_URL=nats://127.0.0.1:4222 \
  /usr/bin/body-ecu-hpc 192.168.100.10
```

You should see:
```
=== Body ECU (AutoSD MPU) ===
[Kuksa] Connected
[NATS] Connected
[VIN] Received from MCU: WVW00000BODYECU01
Body ECU AutoSD MPU ready. Type 'help' for available commands.
ecu>
```

The `ecu>` prompt is an interactive CLI. Type `help` for commands, or `status` to see all vehicle states.

---

## Step 4 — Set Up the Companion App (Mac)

### 4.1 — Create SSH tunnel for NATS

Open a **new terminal** on your Mac and keep it running:

```bash
ssh -L 4222:127.0.0.1:4222 -p 2225 root@127.0.0.1
```

This forwards `localhost:4222` on your Mac to the VM's NATS server.

### 4.2 — Start the Flask web app

In another terminal:

```bash
cd services/companion-app

# First time: set up Python environment
python3 -m venv ../../.venv
source ../../.venv/bin/activate
pip install -r requirements.txt

# Run the app
python app.py --port 5002
```

### 4.3 — Open the Dashboard

Open your browser to: **http://localhost:5002**

You should see the Body ECU dashboard with:
- **Vehicle** card — VIN (auto-detected from MCU)
- **Vehicle Mode** — current ignition state
- **Door Lock** — state + Lock/Unlock buttons
- **Speed** — live speedometer
- **Lighting** — headlight, turn signal, brake light with ON/OFF toggles
- **Event Log** — real-time event stream
- **NATS Log** — protocol-level message trace

---

## Step 5 — Test the Demo

### From the web dashboard

| Action | What happens |
|--------|-------------|
| Click **Lock** | Command flows: Browser → NATS → HPC → Kuksa → SOME/IP → MCU → GPIO |
| Click **Unlock** | Same path in reverse; door state updates on dashboard |
| Click **Headlight ON** | Light toggles on the NUCLEO board; status badge turns green |
| Press the **USER button** on NUCLEO | Ignition mode cycles (OFF → ACC → RUN); dashboard updates |

### From the HPC CLI (in the VM)

```bash
ecu> status          # Show all vehicle states
ecu> door lock       # Lock the door
ecu> door unlock     # Unlock the door
ecu> light head on   # Turn on headlights
ecu> light all off   # Turn off all lights
ecu> mode run        # Set vehicle mode to Run
ecu> speed 120       # Override speed to 120 km/h
ecu> speed clear     # Resume ADC-based speed
```

### Verify with grpcurl (in the VM)

```bash
# Lock the door via Kuksa directly
grpcurl -plaintext -d '{
  "signal_id":{"path":"Vehicle.Command.Door.Lock"},
  "data_point":{"value":{"bool":true}}
}' localhost:55555 kuksa.val.v2.VAL/PublishValue

# Read current vehicle mode
grpcurl -plaintext -d '{
  "signal_id":{"path":"Vehicle.Mode"}
}' localhost:55555 kuksa.val.v2.VAL/GetValue
```

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Dashboard shows "Disconnected" for NATS | SSH tunnel not running | Re-run `ssh -L 4222:...` in a separate terminal |
| Commands from webapp don't reach HPC | SSH tunnel down, or port 4222 already in use | `lsof -i :4222` to check; kill stale processes |
| `Address already in use` on port 5002 | Previous Flask instance running | Kill it: `kill $(lsof -ti :5002)` |
| Kuksa `ParseError` on overlay | Missing `description` in branch nodes | Verify `vss_overlay.json` has descriptions on all branches |
| `error while loading shared libraries` | Wrong RPM or missing bundled libs | Reinstall: `dnf install -y /tmp/body-ecu-hpc-*.rpm` |
| SOME/IP services not found | MCU not connected or wrong IP | Verify Ethernet link; check `MCU_HOST` matches NUCLEO IP |
| No events after startup | SOME/IP SD still searching | Wait ~10s for service discovery; try `status` in CLI |

---

## Architecture Reference

```
body-ecu/
├── app/                          # Zephyr MCU application entry point
├── config/
│   ├── services.yaml             # SOME/IP service definitions (single source of truth)
│   ├── vss_overlay.json          # Custom VSS paths for Kuksa
│   └── can_gateway.yaml          # CAN-SOME/IP mappings
├── libs/
│   ├── body/                     # Domain logic (portable, no platform deps)
│   │   ├── door-lock/
│   │   ├── lighting/
│   │   ├── vehicle-mode/
│   │   ├── speed-simulator/
│   │   └── ignition/
│   ├── platform/                 # ECU-generic services
│   │   ├── ports/                # Abstract interfaces (ISignalBus, ISomeIpService, etc.)
│   │   ├── cloud-gateway/
│   │   ├── diagnostics/
│   │   └── can-gateway/
│   └── adapters/                 # Platform-specific implementations
│       ├── zephyr/               # Zephyr GPIO, CAN, ADC, button adapters
│       ├── autosd/               # Kuksa gRPC, NATS, SomeIpKuksaBridge
│       ├── linux/                # Stub adapters for POSIX development
│       └── system/               # Shared SOME/IP, diagnostics adapters
├── platforms/
│   ├── autosd/                   # AutoSD MPU entry point (production)
│   ├── posix-mcu/                # POSIX MCU simulator
│   └── posix-mpu/                # POSIX MPU simulator
├── packaging/
│   ├── body-ecu-hpc.spec        # RPM spec (bundles gRPC/protobuf libs)
│   ├── body-ecu-hpc.service     # systemd unit file
│   ├── build-rpm.sh             # RPM build script
│   └── Containerfile.rpm-build  # Build container
├── services/
│   └── companion-app/            # Python Flask web dashboard
│       ├── app.py
│       ├── requirements.txt
│       └── test_companion_app.py
├── scripts/
│   └── generate_someip_config.py # Code generator: YAML → C++ headers
└── tests/
    ├── unit/                     # C++ unit tests (GTest)
    └── integration/              # Python E2E tests
```
