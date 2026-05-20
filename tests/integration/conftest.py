"""Shared fixtures for Body ECU integration tests.

These tests require a running Body ECU instance (native_sim or hardware)
and exercise the SOME/IP and diagnostic interfaces end-to-end.

Prerequisites:
    pip install python-someip udsoncan doipclient

Usage (against local posix-mcu binary — default):
    pytest tests/integration/test_lighting.py --mcu-bin=build/posix-mcu/body_ecu_posix_mcu

Usage (against a remote ECU already running):
    pytest tests/integration/ --ecu-host=<ip> [--ecu-port=30490]
"""

import os
import signal
import subprocess

import pytest
import socket
import struct
import time

SOMEIP_PORT = 30490
STARTUP_TIMEOUT = 15.0


def pytest_addoption(parser):
    parser.addoption("--ecu-host", default="127.0.0.1",
                     help="Body ECU IP address")
    parser.addoption("--ecu-port", default=SOMEIP_PORT, type=int,
                     help="Body ECU SOME/IP port")
    parser.addoption("--doip-port", default=13400, type=int,
                     help="Body ECU DoIP TCP port")
    parser.addoption("--mcu-bin",
                     default=os.environ.get("MCU_BIN",
                                            "build/posix-mcu/body_ecu_posix_mcu"),
                     help="Path to posix-mcu binary")
    parser.addoption("--mpu-bin",
                     default=os.environ.get("MPU_BIN",
                                            "build/posix-mpu/body_ecu_posix_mpu"),
                     help="Path to posix-mpu binary")


@pytest.fixture
def ecu_host(request):
    return request.config.getoption("--ecu-host")


@pytest.fixture
def ecu_port(request):
    return request.config.getoption("--ecu-port")


@pytest.fixture
def doip_port(request):
    return request.config.getoption("--doip-port")


# ---- SOME/IP helpers ----

SOMEIP_HEADER_FMT = ">HHIIBBBB"
SOMEIP_HEADER_SIZE = struct.calcsize(SOMEIP_HEADER_FMT)


def build_someip_request(service_id: int, method_id: int,
                         payload: bytes = b"",
                         client_id: int = 0x0042,
                         session_id: int = 0x0001) -> bytes:
    """Build a SOME/IP request message."""
    length = 8 + len(payload)
    client_session = (client_id << 16) | session_id
    return struct.pack(SOMEIP_HEADER_FMT,
                       service_id, method_id, length,
                       client_session,
                       0x01,  # protocol version
                       0x01,  # interface version
                       0x00,  # message type: REQUEST
                       0x00) + payload


def parse_someip_response(data: bytes) -> dict:
    """Parse a SOME/IP response message."""
    header = struct.unpack(SOMEIP_HEADER_FMT, data[:SOMEIP_HEADER_SIZE])
    return {
        "service_id": header[0],
        "method_id": header[1],
        "length": header[2],
        "client_session": header[3],
        "protocol_version": header[4],
        "interface_version": header[5],
        "message_type": header[6],
        "return_code": header[7],
        "payload": data[SOMEIP_HEADER_SIZE:],
    }


def _wait_for_someip(host, port, timeout):
    """Block until the SOME/IP server responds on (host, port)."""
    deadline = time.monotonic() + timeout
    probe = build_someip_request(0x1001, 0x0003)  # DoorLock GetStatus
    while time.monotonic() < deadline:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.settimeout(0.5)
            sock.sendto(probe, (host, port))
            sock.recv(1024)
            sock.close()
            return True
        except (socket.timeout, OSError):
            time.sleep(0.2)
    return False


@pytest.fixture(scope="session")
def running_mcu(request):
    """Start the posix-mcu binary for the test session.

    If --ecu-host points to something other than localhost, assume an
    external ECU is already running and skip the local launch.
    """
    host = request.config.getoption("--ecu-host")
    port = request.config.getoption("--ecu-port")

    if host not in ("127.0.0.1", "localhost", "::1"):
        yield {"host": host, "port": port, "process": None}
        return

    mcu_bin = request.config.getoption("--mcu-bin")
    if not os.path.isfile(mcu_bin):
        pytest.skip(f"MCU binary not found: {mcu_bin}")

    proc = subprocess.Popen(
        [mcu_bin],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env={**os.environ, "SOMEIP_PORT": str(port)},
    )

    if not _wait_for_someip(host, port, STARTUP_TIMEOUT):
        rc = proc.poll()
        out = ""
        if proc.stdout and rc is not None:
            out = proc.stdout.read(4096).decode(errors="replace")
        pytest.fail(
            f"MCU process did not start in time (rc={rc}, output={out[:500]})")

    yield {"host": host, "port": port, "process": proc}

    if proc.poll() is None:
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()


@pytest.fixture
def someip_socket(running_mcu):
    """UDP socket for SOME/IP communication with a running MCU."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)
    sock.connect((running_mcu["host"], running_mcu["port"]))
    yield sock
    sock.close()


# ---- DoIP helpers ----

def build_doip_diagnostic_request(source_addr: int, target_addr: int,
                                  uds_data: bytes) -> bytes:
    """Build a DoIP diagnostic message (type 0x8001)."""
    payload = struct.pack(">HH", source_addr, target_addr) + uds_data
    header = struct.pack(">BBH I",
                         0x02,   # protocol version
                         0xFD,   # inverse version
                         0x8001, # diagnostic message
                         len(payload))
    return header + payload
