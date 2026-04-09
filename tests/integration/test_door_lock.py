"""Integration tests for the Door Lock Service.

Requires a running Body ECU with SOME/IP transport.
Run: pytest tests/integration/test_door_lock.py --ecu-host=<ip>
"""

import pytest
import struct

from conftest import build_someip_request, parse_someip_response

DOOR_LOCK_SERVICE_ID = 0x1001
LOCK_METHOD = 0x0001
UNLOCK_METHOD = 0x0002
GET_STATUS_METHOD = 0x0003
LOCK_STATE_CHANGED = 0x8001

UNLOCKED = 0x00
LOCKED = 0x01


class TestDoorLockService:
    """End-to-end tests for door lock over SOME/IP."""

    def test_lock_door(self, someip_socket):
        """Lock() should transition to Locked state."""
        # Ensure unlocked first
        someip_socket.send(
            build_someip_request(DOOR_LOCK_SERVICE_ID, UNLOCK_METHOD))
        someip_socket.recv(1024)

        # Lock
        request = build_someip_request(DOOR_LOCK_SERVICE_ID, LOCK_METHOD)
        someip_socket.send(request)

        data = someip_socket.recv(1024)
        resp = parse_someip_response(data)

        assert resp["service_id"] == DOOR_LOCK_SERVICE_ID
        assert resp["method_id"] == LOCK_METHOD
        assert resp["return_code"] == 0x00

    def test_unlock_door(self, someip_socket):
        """Unlock() should transition to Unlocked state."""
        # Lock first
        someip_socket.send(
            build_someip_request(DOOR_LOCK_SERVICE_ID, LOCK_METHOD))
        someip_socket.recv(1024)

        # Unlock
        request = build_someip_request(DOOR_LOCK_SERVICE_ID, UNLOCK_METHOD)
        someip_socket.send(request)

        data = someip_socket.recv(1024)
        resp = parse_someip_response(data)

        assert resp["return_code"] == 0x00

    def test_get_status_returns_current_state(self, someip_socket):
        """GetStatus() should return the current lock state."""
        # Lock
        someip_socket.send(
            build_someip_request(DOOR_LOCK_SERVICE_ID, LOCK_METHOD))
        someip_socket.recv(1024)

        # Get status
        request = build_someip_request(DOOR_LOCK_SERVICE_ID, GET_STATUS_METHOD)
        someip_socket.send(request)

        data = someip_socket.recv(1024)
        resp = parse_someip_response(data)

        assert resp["return_code"] == 0x00
        assert resp["payload"][0] == LOCKED

    def test_double_lock_is_noop(self, someip_socket):
        """Calling Lock() twice should succeed without error."""
        someip_socket.send(
            build_someip_request(DOOR_LOCK_SERVICE_ID, LOCK_METHOD))
        someip_socket.recv(1024)

        someip_socket.send(
            build_someip_request(DOOR_LOCK_SERVICE_ID, LOCK_METHOD))
        data = someip_socket.recv(1024)
        resp = parse_someip_response(data)

        assert resp["return_code"] == 0x00

    def test_lock_unlock_lock_cycle(self, someip_socket):
        """Full lock-unlock-lock cycle should work."""
        for method in [LOCK_METHOD, UNLOCK_METHOD, LOCK_METHOD]:
            someip_socket.send(
                build_someip_request(DOOR_LOCK_SERVICE_ID, method))
            data = someip_socket.recv(1024)
            resp = parse_someip_response(data)
            assert resp["return_code"] == 0x00

        # Final state should be Locked
        someip_socket.send(
            build_someip_request(DOOR_LOCK_SERVICE_ID, GET_STATUS_METHOD))
        data = someip_socket.recv(1024)
        resp = parse_someip_response(data)
        assert resp["payload"][0] == LOCKED
