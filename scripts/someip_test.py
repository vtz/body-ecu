#!/usr/bin/env python3
"""
SOME/IP test script via Jumpstarter.

Discovers services on the ECU, makes RPC calls to the door lock service,
and subscribes to event groups to log incoming notifications.

Usage:
    jmp shell --exporter-config export.yaml --direct
    Then inside the shell:
        run scripts/someip_test.py

    Or standalone:
        jmp run --exporter-config export.yaml --direct -- python3 scripts/someip_test.py
"""

import sys
import struct
import logging
from jumpstarter.common.utils import env

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)
log = logging.getLogger("someip-test")

DOOR_LOCK_SERVICE = 0x1001
LOCK_METHOD = 0x0001
UNLOCK_METHOD = 0x0002
GET_STATUS_METHOD = 0x0003
DOOR_EVENT_GROUP = 0x0001

SPEED_SERVICE = 0x1003
GET_SPEED_METHOD = 0x0001
SPEED_EVENT_GROUP = 0x0001


def discover_services(someip):
    log.info("--- Service Discovery ---")
    for sid in [DOOR_LOCK_SERVICE, SPEED_SERVICE]:
        try:
            services = someip.find_service(sid, timeout=3.0)
            for svc in services:
                log.info(
                    "Found service=%#06x instance=%#06x",
                    svc.service_id, svc.instance_id,
                )
        except Exception as e:
            log.warning("SD for service %#06x failed: %s", sid, e)


def test_door_lock(someip):
    log.info("--- Door Lock RPC ---")

    resp = someip.rpc_call(DOOR_LOCK_SERVICE, LOCK_METHOD, b"", timeout=5.0)
    log.info("LOCK  -> return_code=%d payload=[%s]", resp.return_code, resp.payload)

    resp = someip.rpc_call(DOOR_LOCK_SERVICE, GET_STATUS_METHOD, b"", timeout=5.0)
    payload_hex = resp.payload or ""
    status = bytes.fromhex(payload_hex) if payload_hex else b""
    log.info(
        "STATUS -> %s (raw=%s)",
        "LOCKED" if status == b"\x01" else "UNLOCKED" if status == b"\x00" else repr(status),
        payload_hex,
    )

    resp = someip.rpc_call(DOOR_LOCK_SERVICE, UNLOCK_METHOD, b"", timeout=5.0)
    log.info("UNLOCK -> return_code=%d payload=[%s]", resp.return_code, resp.payload or "")

    resp = someip.rpc_call(DOOR_LOCK_SERVICE, GET_STATUS_METHOD, b"", timeout=5.0)
    payload_hex = resp.payload or ""
    status = bytes.fromhex(payload_hex) if payload_hex else b""
    log.info(
        "STATUS -> %s (raw=%s)",
        "LOCKED" if status == b"\x01" else "UNLOCKED" if status == b"\x00" else repr(status),
        payload_hex,
    )


def test_speed(someip):
    log.info("--- Speed RPC ---")
    try:
        resp = someip.rpc_call(SPEED_SERVICE, GET_SPEED_METHOD, b"", timeout=5.0)
        payload_hex = resp.payload or ""
        payload = bytes.fromhex(payload_hex) if payload_hex else b""
        if len(payload) == 4:
            speed = struct.unpack(">f", payload)[0]
            log.info("Speed: %.1f km/h (raw=%s)", speed, payload_hex)
        else:
            log.info("Speed response: raw=%s (%d bytes)", payload_hex, len(payload))
    except Exception as e:
        log.warning("GetSpeed failed: %s", e)


def listen_events(someip, duration=600):
    log.info("--- Subscribing to events (listening for %ds) ---", duration)
    someip.subscribe_eventgroup(DOOR_EVENT_GROUP)
    try:
        while True:
            try:
                event = someip.receive_event(timeout=float(duration))
                payload = bytes.fromhex(event.payload) if event.payload else b""
                log.info(
                    "EVENT service=%#06x event=%#06x payload=%s",
                    event.service_id, event.event_id, payload.hex(),
                )
            except TimeoutError:
                log.info("No more events within timeout")
                break
            except Exception as e:
                log.warning("receive_event error: %s", e)
                break
    finally:
        someip.unsubscribe_eventgroup(DOOR_EVENT_GROUP)


def main():
    log.info("Connecting to ECU via Jumpstarter SOME/IP driver...")

    with env() as client:
        someip = client.someip

        discover_services(someip)
        test_door_lock(someip)
        test_speed(someip)
        listen_events(someip)

    log.info("Done.")


if __name__ == "__main__":
    main()
