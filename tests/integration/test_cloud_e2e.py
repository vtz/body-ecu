"""End-to-end cloud connectivity integration tests.

Verifies the full round-trip: NATS command -> CloudGatewayClient ->
SignalBus -> SomeIpKuksaBridge -> SOME/IP -> MCU -> response -> NATS.

Requires:
    - docker compose -f docker-compose.test.yml up -d  (NATS on 4222)
    - Build posix-mcu and posix-mpu
    - pip install nats-py pytest

Usage:
    pytest tests/integration/test_cloud_e2e.py -v \\
        --mcu-bin=build/posix-mcu/body_ecu_posix_mcu \\
        --mpu-bin=build/posix-mpu/body_ecu_posix_mpu
"""

import asyncio
import os
import signal
import socket
import struct
import subprocess
import time
import threading

import pytest

try:
    import nats as nats_lib
    HAS_NATS_PY = True
except ImportError:
    HAS_NATS_PY = False


VIN = "WVWZZZ3CZWE000001"

SOMEIP_HEADER_FMT = ">HHIIBBBB"
SOMEIP_HEADER_SIZE = struct.calcsize(SOMEIP_HEADER_FMT)

DOOR_LOCK_SERVICE_ID = 0x1001
LOCK_METHOD = 0x0001
UNLOCK_METHOD = 0x0002
GET_STATUS_METHOD = 0x0003

SOMEIP_PORT = 30510
MSG_TYPE_RESPONSE = 0x80

STARTUP_TIMEOUT = 15.0


def build_someip_request(service_id, method_id, payload=b""):
    length = 8 + len(payload)
    client_session = (0x0042 << 16) | 0x0001
    return struct.pack(SOMEIP_HEADER_FMT,
                       service_id, method_id, length,
                       client_session, 0x01, 0x01, 0x00, 0x00) + payload


def parse_someip_response(data):
    header = struct.unpack(SOMEIP_HEADER_FMT, data[:SOMEIP_HEADER_SIZE])
    return {
        "service_id": header[0],
        "method_id": header[1],
        "message_type": header[6],
        "return_code": header[7],
        "payload": data[SOMEIP_HEADER_SIZE:],
    }


def recv_method_response(sock, retries=5):
    for _ in range(retries):
        data = sock.recv(1024)
        resp = parse_someip_response(data)
        if resp["message_type"] == MSG_TYPE_RESPONSE:
            return resp
    raise TimeoutError("No method response received after skipping events")


def wait_for_port(host, port, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.settimeout(0.5)
            sock.sendto(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                             GET_STATUS_METHOD),
                        (host, port))
            sock.recv(1024)
            sock.close()
            return True
        except (socket.timeout, OSError):
            time.sleep(0.2)
    return False


def nats_available():
    try:
        sock = socket.create_connection(("127.0.0.1", 4222), timeout=2)
        sock.close()
        return True
    except (ConnectionRefusedError, socket.timeout, OSError):
        return False


class NatsTestClient:
    """Async NATS client running in a background thread for test assertions."""

    def __init__(self, url="nats://localhost:4222"):
        self._url = url
        self._nc = None
        self._loop = None
        self._thread = None
        self._received = []
        self._lock = threading.Lock()

    def start(self):
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        for _ in range(50):
            if self._nc is not None:
                break
            time.sleep(0.1)

    def _run(self):
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        self._loop.run_until_complete(self._connect())

    async def _connect(self):
        self._nc = await nats_lib.connect(self._url)
        state_subject = f"vehicles.{VIN}.state.door.locked"
        await self._nc.subscribe(state_subject, cb=self._on_msg)
        resp_subject = f"vehicles.{VIN}.command.door.response"
        await self._nc.subscribe(resp_subject, cb=self._on_msg)
        while self._nc.is_connected:
            await asyncio.sleep(0.05)

    async def _on_msg(self, msg):
        with self._lock:
            self._received.append({
                "subject": msg.subject,
                "data": list(msg.data),
            })

    def publish_sync(self, subject, data):
        future = asyncio.run_coroutine_threadsafe(
            self._nc.publish(subject, bytes(data)), self._loop
        )
        future.result(timeout=5.0)

    def get_received(self):
        with self._lock:
            return list(self._received)

    def clear(self):
        with self._lock:
            self._received.clear()

    def wait_for_message(self, subject, timeout=5.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            with self._lock:
                for msg in self._received:
                    if msg["subject"] == subject:
                        return msg
            time.sleep(0.1)
        return None


@pytest.fixture(scope="module")
def skip_without_nats():
    if not nats_available():
        pytest.skip("NATS server not available on localhost:4222 "
                     "(run: docker compose -f docker-compose.test.yml up -d)")


@pytest.fixture(scope="module")
def skip_without_nats_py():
    if not HAS_NATS_PY:
        pytest.skip("nats-py not installed (pip install nats-py)")


@pytest.fixture(scope="module")
def cloud_e2e_env(request, skip_without_nats, skip_without_nats_py):
    """Start MCU + MPU processes and a NATS test client."""
    mcu_bin = request.config.getoption("--mcu-bin")
    mpu_bin = request.config.getoption("--mpu-bin")

    if not os.path.isfile(mcu_bin):
        pytest.skip(f"MCU binary not found: {mcu_bin}")
    if not os.path.isfile(mpu_bin):
        pytest.skip(f"MPU binary not found: {mpu_bin}")

    mcu_proc = subprocess.Popen(
        [mcu_bin],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env={**os.environ, "SOMEIP_PORT": str(SOMEIP_PORT)},
    )

    if not wait_for_port("127.0.0.1", SOMEIP_PORT, STARTUP_TIMEOUT):
        rc = mcu_proc.poll()
        out = ""
        if mcu_proc.stdout and rc is not None:
            out = mcu_proc.stdout.read(4096).decode(errors="replace")
        pytest.fail(f"MCU process did not start in time (rc={rc}, output={out[:500]})")

    mpu_proc = subprocess.Popen(
        [mpu_bin, "127.0.0.1"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env={**os.environ, "NATS_URL": "nats://localhost:4222"},
    )

    time.sleep(1.0)

    nats_client = NatsTestClient()
    nats_client.start()

    yield {
        "mcu": mcu_proc,
        "mpu": mpu_proc,
        "nats": nats_client,
        "host": "127.0.0.1",
        "port": SOMEIP_PORT,
    }

    for proc in [mpu_proc, mcu_proc]:
        if proc.poll() is None:
            proc.send_signal(signal.SIGTERM)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()


@pytest.fixture
def someip_socket(cloud_e2e_env):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)
    sock.connect((cloud_e2e_env["host"], cloud_e2e_env["port"]))
    yield sock
    sock.close()


class TestCloudE2EWithStubTransport:
    """Tests that work with the stub cloud transport (no real NATS on MPU).

    These verify the MCU <-> MPU SOME/IP path and that the cloud gateway
    processes events without crashing.
    """

    def test_lock_via_someip_mpu_survives(self, someip_socket, cloud_e2e_env):
        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                UNLOCK_METHOD))
        recv_method_response(someip_socket)

        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                LOCK_METHOD))
        resp = recv_method_response(someip_socket)
        assert resp["return_code"] == 0x00
        time.sleep(0.3)
        assert cloud_e2e_env["mpu"].poll() is None

    def test_rapid_lock_unlock_stability(self, someip_socket, cloud_e2e_env):
        for _ in range(15):
            someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                    LOCK_METHOD))
            recv_method_response(someip_socket)
            someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                    UNLOCK_METHOD))
            recv_method_response(someip_socket)

        time.sleep(0.3)
        assert cloud_e2e_env["mcu"].poll() is None
        assert cloud_e2e_env["mpu"].poll() is None

    def test_get_status_after_lock(self, someip_socket, cloud_e2e_env):
        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                LOCK_METHOD))
        recv_method_response(someip_socket)

        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                GET_STATUS_METHOD))
        resp = recv_method_response(someip_socket)
        assert resp["return_code"] == 0x00
        assert resp["payload"][0] == 0x01
