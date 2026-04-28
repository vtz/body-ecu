#!/usr/bin/env python3
"""Tests for generate_someip_config.py"""

import os
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(__file__))
from generate_someip_config import emit_service_ids, emit_mpu_config

import yaml

SAMPLE_YAML = textwrap.dedent("""\
    network:
      host: "0.0.0.0"
      port: 30490

    services:
      lighting:
        service_id: 0x1000
        instance_id: 0x0001
        methods:
          set_light_state: 0x0001
          get_light_status: 0x0002
        events:
          light_status_changed: 0x8001
        eventgroups:
          lighting_events: 0x0001

      cloud_gateway:
        vin: "WVWZZZ3CZWE000001"
        nats_url: "nats://localhost:4222"

      vehicle_mode:
        service_id: 0x1002
        instance_id: 0x0001
        fields:
          mode:
            getter: 0x0001
            setter: 0x0002
            notifier: 0x8001
        eventgroups:
          mode_events: 0x0001
""")


class TestEmitServiceIds(unittest.TestCase):
    def setUp(self):
        config = yaml.safe_load(SAMPLE_YAML)
        self.services = config["services"]
        self.output = emit_service_ids(self.services)

    def test_contains_pragma_once(self):
        self.assertIn("#pragma once", self.output)

    def test_contains_namespace(self):
        self.assertIn("namespace body_ecu::someip", self.output)

    def test_lighting_service_id(self):
        self.assertIn("kServiceId  = 0x1000", self.output)

    def test_lighting_method(self):
        self.assertIn("kSetLightState = 0x0001", self.output)
        self.assertIn("kGetLightStatus = 0x0002", self.output)

    def test_lighting_event(self):
        self.assertIn("kLightStatusChanged = 0x8001", self.output)

    def test_lighting_eventgroup(self):
        self.assertIn("kLightingEvents = 0x0001", self.output)

    def test_skips_cloud_gateway(self):
        self.assertNotIn("cloud_gateway", self.output)

    def test_vehicle_mode_fields(self):
        self.assertIn("kModeGetter", self.output)
        self.assertIn("kModeSetter", self.output)
        self.assertIn("kModeNotifier", self.output)

    def test_vehicle_mode_uses_field_namespace(self):
        self.assertIn("namespace field", self.output)


class TestEmitMpuConfig(unittest.TestCase):
    def setUp(self):
        config = yaml.safe_load(SAMPLE_YAML)
        self.services = config["services"]
        self.output = emit_mpu_config(self.services)

    def test_includes_ids_header(self):
        self.assertIn('#include "someip_service_ids.h"', self.output)

    def test_struct_name(self):
        self.assertIn("struct MpuClientConfig", self.output)

    def test_lighting_fields(self):
        self.assertIn("lighting_service_id = lighting::kServiceId", self.output)
        self.assertIn("lighting::method::kSetLightState", self.output)

    def test_skips_cloud_gateway(self):
        self.assertNotIn("cloud_gateway", self.output)

    def test_vehicle_mode_getter_setter(self):
        self.assertIn("vehicle_mode_mode_getter", self.output)
        self.assertIn("vehicle_mode_mode_setter", self.output)


class TestEndToEnd(unittest.TestCase):
    def test_generates_files(self):
        config = yaml.safe_load(SAMPLE_YAML)
        services = config["services"]

        with tempfile.TemporaryDirectory() as tmpdir:
            ids_path = Path(tmpdir) / "someip_service_ids.h"
            mpu_path = Path(tmpdir) / "someip_mpu_config.h"

            ids_path.write_text(emit_service_ids(services))
            mpu_path.write_text(emit_mpu_config(services))

            self.assertTrue(ids_path.exists())
            self.assertTrue(mpu_path.exists())
            self.assertGreater(ids_path.stat().st_size, 0)
            self.assertGreater(mpu_path.stat().st_size, 0)

    def test_hex_formatting(self):
        config = yaml.safe_load(SAMPLE_YAML)
        services = config["services"]
        output = emit_service_ids(services)
        self.assertIn("0x1000", output)
        self.assertIn("0x0001", output)
        self.assertIn("0x8001", output)


class TestSnakeToPascal(unittest.TestCase):
    def test_simple(self):
        from generate_someip_config import snake_to_pascal
        self.assertEqual(snake_to_pascal("set_light_state"), "SetLightState")
        self.assertEqual(snake_to_pascal("lock"), "Lock")
        self.assertEqual(snake_to_pascal("get_status"), "GetStatus")


if __name__ == "__main__":
    unittest.main()
