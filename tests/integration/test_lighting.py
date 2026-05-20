"""Integration tests for the Exterior Lighting Service.

Requires a running Body ECU with SOME/IP transport.
Run: pytest tests/integration/test_lighting.py --ecu-host=<ip>
"""

import pytest
import struct
import time

from conftest import build_someip_request, parse_someip_response, recv_method_response

LIGHTING_SERVICE_ID = 0x1000
SET_LIGHT_STATE = 0x0001
GET_LIGHT_STATUS = 0x0002
LIGHT_STATUS_CHANGED = 0x8001


class TestLightingService:
    """End-to-end tests for exterior lighting over SOME/IP."""

    def test_set_headlight_on(self, someip_socket):
        """SetLightState(HEADLIGHT, ON) should succeed."""
        payload = struct.pack("BB", 0x00, 0x01)  # light_id=0 (Headlight), state=ON
        request = build_someip_request(LIGHTING_SERVICE_ID, SET_LIGHT_STATE, payload)
        someip_socket.send(request)

        resp = recv_method_response(someip_socket)

        assert resp["service_id"] == LIGHTING_SERVICE_ID
        assert resp["method_id"] == SET_LIGHT_STATE
        assert resp["message_type"] == 0x80  # Response
        assert resp["return_code"] == 0x00   # E_OK

    def test_get_light_status(self, someip_socket):
        """GetLightStatus should return current state of all lights."""
        set_payload = struct.pack("BB", 0x00, 0x01)  # Headlight ON
        someip_socket.send(
            build_someip_request(LIGHTING_SERVICE_ID, SET_LIGHT_STATE, set_payload))
        recv_method_response(someip_socket)

        time.sleep(0.1)

        request = build_someip_request(LIGHTING_SERVICE_ID, GET_LIGHT_STATUS)
        someip_socket.send(request)

        resp = recv_method_response(someip_socket)

        assert resp["service_id"] == LIGHTING_SERVICE_ID
        assert resp["method_id"] == GET_LIGHT_STATUS
        assert resp["message_type"] == 0x80
        assert resp["return_code"] == 0x00
        assert len(resp["payload"]) >= 3  # 3 light states

    def test_set_invalid_light_id(self, someip_socket):
        """SetLightState with invalid light_id should return error."""
        payload = struct.pack("BB", 0xFF, 0x01)  # Invalid light_id
        request = build_someip_request(LIGHTING_SERVICE_ID, SET_LIGHT_STATE, payload)
        someip_socket.send(request)

        resp = recv_method_response(someip_socket)

        assert resp["return_code"] != 0x00  # Should indicate error

    def test_set_all_lights_sequence(self, someip_socket):
        """Turn on each light sequentially, verify status reflects all ON."""
        for light_id in range(3):
            payload = struct.pack("BB", light_id, 0x01)
            someip_socket.send(
                build_someip_request(LIGHTING_SERVICE_ID, SET_LIGHT_STATE, payload))
            recv_method_response(someip_socket)

        time.sleep(0.1)

        request = build_someip_request(LIGHTING_SERVICE_ID, GET_LIGHT_STATUS)
        someip_socket.send(request)

        resp = recv_method_response(someip_socket)
        assert all(b == 0x01 for b in resp["payload"][:3])
