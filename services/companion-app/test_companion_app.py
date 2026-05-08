"""Unit tests for the companion app web service.

These test the Flask routes and VehicleState logic without needing
a running NATS server. The NATS client is not started for route tests.
"""

import json
import unittest
from unittest.mock import patch, MagicMock

from app import create_app, VehicleState, NatsClient


class TestVehicleState(unittest.TestCase):
    def test_initial_state(self):
        state = VehicleState()
        self.assertIsNone(state.door_locked)
        self.assertIsNone(state.last_command_response)
        self.assertIsNone(state.vehicle_mode)
        self.assertIsNone(state.speed_kmh)
        self.assertIsNone(state.light_status)
        self.assertEqual(state.events, [])
        self.assertEqual(state.logs, [])

    def test_events_accumulate(self):
        state = VehicleState()
        state.add_event({"type": "test"})
        state.add_event({"type": "test2"})
        self.assertEqual(len(state.events), 2)
        self.assertIn("ts", state.events[0])

    def test_logs_accumulate(self):
        state = VehicleState()
        state.add_log("hello")
        state.add_log("world")
        self.assertEqual(len(state.logs), 2)
        self.assertEqual(state.logs[0]["msg"], "hello")


class TestFlaskRoutes(unittest.TestCase):
    """Test Flask routes with a mock NATS client."""

    def setUp(self):
        self.app = create_app("nats://localhost:4222")
        self.app.config["TESTING"] = True
        self.client = self.app.test_client()

    def test_health_endpoint(self):
        resp = self.client.get("/health")
        data = json.loads(resp.data)
        self.assertIn("healthy", data)
        self.assertIn("nats_url", data)

    def test_get_state_initial(self):
        resp = self.client.get("/api/state")
        data = json.loads(resp.data)
        self.assertIsNone(data["door_locked"])
        self.assertIsNone(data["last_command_response"])
        self.assertIsNone(data["vehicle_mode"])
        self.assertIsNone(data["speed_kmh"])
        self.assertIsNone(data["light_status"])

    def test_get_events_empty(self):
        resp = self.client.get("/api/events")
        data = json.loads(resp.data)
        self.assertEqual(data["events"], [])

    def test_get_logs(self):
        resp = self.client.get("/api/logs")
        data = json.loads(resp.data)
        self.assertIsInstance(data["logs"], list)

    def test_dashboard_renders(self):
        resp = self.client.get("/")
        self.assertEqual(resp.status_code, 200)
        self.assertIn(b"Body ECU", resp.data)

    def test_clear_events(self):
        resp = self.client.delete("/api/events")
        data = json.loads(resp.data)
        self.assertTrue(data["cleared"])


if __name__ == "__main__":
    unittest.main()
