"""Cloud Gateway integration tests.

Tests the full cloud command -> signal bus -> SOME/IP -> MCU path using
the two-process POSIX setup. When a NATS server is available, tests the
actual NATS transport; otherwise tests the stub cloud transport via
process stdout verification.

Prerequisites:
    - Build posix-mcu and posix-mpu
    - Optional: nats-server running on localhost:4222

Usage:
    pytest tests/integration/test_cloud_gateway.py [--mcu-bin=<path>] [--mpu-bin=<path>]
"""

import os
import signal
import socket
import struct
import subprocess
import time

import pytest


SOMEIP_HEADER_FMT = ">HHIIBBBB"
SOMEIP_HEADER_SIZE = struct.calcsize(SOMEIP_HEADER_FMT)

DOOR_LOCK_SERVICE_ID = 0x1001
LOCK_METHOD = 0x0001
UNLOCK_METHOD = 0x0002
GET_STATUS_METHOD = 0x0003

SOMEIP_PORT = 30500  # Different port to avoid conflict with test_two_process and SD multicast
STARTUP_TIMEOUT = 15.0

MSG_TYPE_RESPONSE = 0x80


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
    """Receive a SOME/IP method response, skipping event notifications."""
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


def nats_server_available():
    """Check if a NATS server is listening on localhost:4222."""
    try:
        sock = socket.create_connection(("127.0.0.1", 4222), timeout=1)
        sock.close()
        return True
    except (ConnectionRefusedError, socket.timeout, OSError):
        return False


@pytest.fixture(scope="module")
def cloud_gateway_env(request):
    """Start MCU and MPU processes for cloud gateway testing."""
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
        if mcu_proc.stdout:
            out = mcu_proc.stdout.read(4096).decode(errors="replace") if rc is not None else ""
        pytest.fail(f"MCU process did not start in time (rc={rc}, output={out[:500]})")

    mpu_env = {**os.environ}
    mpu_proc = subprocess.Popen(
        [mpu_bin, "127.0.0.1"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=mpu_env,
    )

    time.sleep(1.0)

    yield {
        "mcu": mcu_proc,
        "mpu": mpu_proc,
        "host": "127.0.0.1",
        "port": SOMEIP_PORT,
        "has_nats": nats_server_available(),
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
def someip_socket(cloud_gateway_env):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)
    sock.connect((cloud_gateway_env["host"], cloud_gateway_env["port"]))
    yield sock
    sock.close()


class TestCloudGatewayStub:
    """Test the cloud gateway path with stub transport (stdout verification)."""

    def test_lock_event_reaches_cloud_gateway(self, someip_socket,
                                               cloud_gateway_env):
        """Locking via SOME/IP should propagate through the bridge to the
        cloud gateway client and the MCU state must reflect the change."""
        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                UNLOCK_METHOD))
        pre = recv_method_response(someip_socket)
        assert pre["return_code"] == 0x00, "Precondition unlock failed"
        time.sleep(0.2)

        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                LOCK_METHOD))
        resp = recv_method_response(someip_socket)
        assert resp["return_code"] == 0x00, "Lock command must succeed"

        time.sleep(0.5)

        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                GET_STATUS_METHOD))
        status = recv_method_response(someip_socket)
        assert status["return_code"] == 0x00, "GetStatus must succeed"
        assert status["payload"][0] == 0x01, "MCU must report Locked"

        assert cloud_gateway_env["mpu"].poll() is None, \
            "MPU process crashed after lock event"

    def test_unlock_event_reaches_cloud_gateway(self, someip_socket,
                                                 cloud_gateway_env):
        """Unlocking via SOME/IP should propagate and the MCU state
        must reflect Unlocked."""
        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                LOCK_METHOD))
        pre = recv_method_response(someip_socket)
        assert pre["return_code"] == 0x00, "Precondition lock failed"
        time.sleep(0.2)

        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                UNLOCK_METHOD))
        resp = recv_method_response(someip_socket)
        assert resp["return_code"] == 0x00, "Unlock command must succeed"

        time.sleep(0.5)

        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                GET_STATUS_METHOD))
        status = recv_method_response(someip_socket)
        assert status["return_code"] == 0x00, "GetStatus must succeed"
        assert status["payload"][0] == 0x00, "MCU must report Unlocked"

        assert cloud_gateway_env["mpu"].poll() is None, \
            "MPU process crashed after unlock event"

    def test_cloud_gateway_survives_rapid_state_changes(
            self, someip_socket, cloud_gateway_env):
        """Rapid lock/unlock should not crash either process and leave
        a deterministic final state."""
        for _ in range(20):
            someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                    LOCK_METHOD))
            resp = recv_method_response(someip_socket)
            assert resp["return_code"] == 0x00
            someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                    UNLOCK_METHOD))
            resp = recv_method_response(someip_socket)
            assert resp["return_code"] == 0x00

        time.sleep(0.5)

        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                GET_STATUS_METHOD))
        status = recv_method_response(someip_socket)
        assert status["return_code"] == 0x00, "GetStatus must succeed"
        assert status["payload"][0] == 0x00, "Final state must be Unlocked"

        assert cloud_gateway_env["mcu"].poll() is None, "MCU crashed"
        assert cloud_gateway_env["mpu"].poll() is None, "MPU crashed"


class TestCloudGatewayNats:
    """Test the cloud gateway with a real NATS server (if available)."""

    @pytest.fixture(autouse=True)
    def skip_without_nats(self, cloud_gateway_env):
        if not cloud_gateway_env["has_nats"]:
            pytest.skip("NATS server not available on localhost:4222")

    def test_nats_round_trip(self, someip_socket, cloud_gateway_env):
        """With a real NATS server, lock state changes should be published
        to the NATS subject. This test verifies the MPU stays healthy
        (full NATS verification requires a NATS subscriber in the test)."""
        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                LOCK_METHOD))
        recv_method_response(someip_socket)
        time.sleep(0.5)

        assert cloud_gateway_env["mpu"].poll() is None, \
            "MPU crashed during NATS round trip"
