"""Shared fixtures for Body ECU integration tests.

These tests require a running Body ECU instance (native_sim or hardware)
and exercise the SOME/IP and diagnostic interfaces end-to-end.

Prerequisites:
    pip install python-someip udsoncan doipclient

Usage:
    pytest tests/integration/ --ecu-host=<ip> [--ecu-port=30490]
"""

import os

import pytest
import socket
import struct
import time


def pytest_addoption(parser):
    parser.addoption("--ecu-host", default="127.0.0.1",
                     help="Body ECU IP address")
    parser.addoption("--ecu-port", default=30490, type=int,
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

SOMEIP_HEADER_FMT = ">HHIHBBH"  # service_id, method_id, length, client/session, proto/iface, msg_type, return_code
SOMEIP_HEADER_SIZE = struct.calcsize(SOMEIP_HEADER_FMT)


def build_someip_request(service_id: int, method_id: int,
                         payload: bytes = b"",
                         client_id: int = 0x0042,
                         session_id: int = 0x0001) -> bytes:
    """Build a SOME/IP request message."""
    length = 8 + len(payload)  # request_id(4) + proto/iface(1) + msg_type(1) + return_code(2) + payload
    client_session = (client_id << 16) | session_id
    return struct.pack(SOMEIP_HEADER_FMT,
                       service_id, method_id, length,
                       client_session,
                       0x01,  # protocol version
                       0x00,  # message type: REQUEST
                       0x0000) + payload


def parse_someip_response(data: bytes) -> dict:
    """Parse a SOME/IP response message."""
    header = struct.unpack(SOMEIP_HEADER_FMT, data[:SOMEIP_HEADER_SIZE])
    return {
        "service_id": header[0],
        "method_id": header[1],
        "length": header[2],
        "client_session": header[3],
        "protocol_version": header[4],
        "message_type": header[5],
        "return_code": header[6],
        "payload": data[SOMEIP_HEADER_SIZE:],
    }


@pytest.fixture
def someip_socket(ecu_host, ecu_port):
    """UDP socket for SOME/IP communication."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)
    sock.connect((ecu_host, ecu_port))
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
