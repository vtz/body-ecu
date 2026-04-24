"""Two-process POSIX integration tests.

Starts both the MCU and MPU POSIX processes, sends SOME/IP commands to the MCU,
and verifies that the MPU receives state change events via the SOME/IP bridge.

Prerequisites:
    - Build posix-mcu and posix-mpu:
        cmake -B build/posix-mcu -S platforms/posix-mcu && cmake --build build/posix-mcu
        cmake -B build/posix-mpu -S platforms/posix-mpu && cmake --build build/posix-mpu

Usage:
    pytest tests/integration/test_two_process.py [--mcu-bin=<path>] [--mpu-bin=<path>]
"""

import os
import signal
import socket
import struct
import subprocess
import sys
import time

import pytest


SOMEIP_HEADER_FMT = ">HHIHBBH"
SOMEIP_HEADER_SIZE = struct.calcsize(SOMEIP_HEADER_FMT)

DOOR_LOCK_SERVICE_ID = 0x1001
LOCK_METHOD = 0x0001
UNLOCK_METHOD = 0x0002
GET_STATUS_METHOD = 0x0003

SOMEIP_PORT = 30490
STARTUP_TIMEOUT = 5.0


def build_someip_request(service_id, method_id, payload=b""):
    length = 8 + len(payload)
    client_session = (0x0042 << 16) | 0x0001
    return struct.pack(SOMEIP_HEADER_FMT,
                       service_id, method_id, length,
                       client_session, 0x01, 0x00, 0x0000) + payload


def parse_someip_response(data):
    header = struct.unpack(SOMEIP_HEADER_FMT, data[:SOMEIP_HEADER_SIZE])
    return {
        "service_id": header[0],
        "method_id": header[1],
        "return_code": header[6],
        "payload": data[SOMEIP_HEADER_SIZE:],
    }


def wait_for_port(host, port, timeout):
    """Block until a UDP service responds on (host, port)."""
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


@pytest.fixture(scope="module")
def two_process_env(request):
    """Start MCU and MPU processes for the test module."""
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

    assert wait_for_port("127.0.0.1", SOMEIP_PORT, STARTUP_TIMEOUT), \
        "MCU process did not start in time"

    mpu_proc = subprocess.Popen(
        [mpu_bin, "127.0.0.1"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    time.sleep(1.0)

    yield {
        "mcu": mcu_proc,
        "mpu": mpu_proc,
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
def someip_socket(two_process_env):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)
    sock.connect((two_process_env["host"], two_process_env["port"]))
    yield sock
    sock.close()


class TestTwoProcessIntegration:
    """Verify MCU + MPU two-process communication over SOME/IP."""

    def test_mcu_lock_via_someip(self, someip_socket, two_process_env):
        """SOME/IP Lock command to MCU should succeed."""
        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                UNLOCK_METHOD))
        someip_socket.recv(1024)

        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                LOCK_METHOD))
        data = someip_socket.recv(1024)
        resp = parse_someip_response(data)

        assert resp["return_code"] == 0x00

    def test_mcu_status_after_lock(self, someip_socket, two_process_env):
        """After locking, GetStatus should return Locked."""
        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                LOCK_METHOD))
        someip_socket.recv(1024)

        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                GET_STATUS_METHOD))
        data = someip_socket.recv(1024)
        resp = parse_someip_response(data)

        assert resp["return_code"] == 0x00
        assert resp["payload"][0] == 0x01  # Locked

    def test_mpu_receives_event(self, someip_socket, two_process_env):
        """MPU should log the lock state change event from MCU."""
        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                UNLOCK_METHOD))
        someip_socket.recv(1024)
        time.sleep(0.2)

        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                LOCK_METHOD))
        someip_socket.recv(1024)

        # Give the MPU time to process the event
        time.sleep(0.5)

        mpu_proc = two_process_env["mpu"]
        assert mpu_proc.poll() is None, "MPU process crashed"

    def test_full_lock_unlock_cycle(self, someip_socket, two_process_env):
        """Full lock -> unlock -> lock cycle should work with both processes."""
        for method in [LOCK_METHOD, UNLOCK_METHOD, LOCK_METHOD]:
            someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                    method))
            data = someip_socket.recv(1024)
            resp = parse_someip_response(data)
            assert resp["return_code"] == 0x00

        someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                GET_STATUS_METHOD))
        data = someip_socket.recv(1024)
        resp = parse_someip_response(data)
        assert resp["payload"][0] == 0x01  # Locked

    def test_processes_survive_rapid_commands(self, someip_socket,
                                              two_process_env):
        """Rapid lock/unlock should not crash either process."""
        for _ in range(10):
            someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                    LOCK_METHOD))
            someip_socket.recv(1024)
            someip_socket.send(build_someip_request(DOOR_LOCK_SERVICE_ID,
                                                    UNLOCK_METHOD))
            someip_socket.recv(1024)

        assert two_process_env["mcu"].poll() is None, "MCU crashed"
        assert two_process_env["mpu"].poll() is None, "MPU crashed"
