"""Status and RC regression checks; no real USB device or aircraft traffic."""

import asyncio
import json
import unittest
from types import SimpleNamespace
from unittest.mock import Mock, patch

import bridge_status
import control_bridge
import evdev


class BridgeTest(unittest.IsolatedAsyncioTestCase):
    async def test_status_publisher_survives_missing_receiver(self):
        status = bridge_status.BridgeStatus()
        delivered = asyncio.Event()
        packets = []

        def send(data, address):
            self.assertEqual(address, ("127.0.0.1", 17002))
            packets.append(json.loads(data))
            if len(packets) == 1:
                status.set("ready")
                raise BlockingIOError()
            delivered.set()

        sock = Mock()
        sock.sendto.side_effect = send
        factory = Mock()
        factory.return_value.__enter__ = Mock(return_value=sock)
        factory.return_value.__exit__ = Mock(return_value=False)
        with patch.object(bridge_status.socket, "socket", factory):
            task = asyncio.create_task(status.publish())
            try:
                await asyncio.wait_for(delivered.wait(), 2)
                self.assertEqual(packets[0]["pocket"]["state"], "disconnected")
                self.assertEqual(packets[1]["pocket"]["state"], "ready")
                self.assertEqual(packets[1]["v"], 1)
                self.assertEqual(packets[1]["type"], "qgc_control_bridge")
                sock.setblocking.assert_called_once_with(False)
            finally:
                task.cancel()
                await asyncio.gather(task, return_exceptions=True)

    async def test_original_rc_mapping_and_clean_cancellation(self):
        sent = asyncio.Event()
        reader_closed = asyncio.Event()

        async def events():
            try:
                await asyncio.Future()
                yield  # Keep an idle controller connected without stick movement.
            finally:
                reader_closed.set()

        dev = Mock()
        dev.async_read_loop = events
        values = {0: -100, 1: 100, 2: 0, 3: 50, 4: -50, 5: 100, 6: -100, 7: 0}
        dev.capabilities.return_value = {
            evdev.ecodes.EV_ABS: [
                (code, SimpleNamespace(value=value, min=-100, max=100))
                for code, value in values.items()
            ]
        }
        dev.active_keys.return_value = [304]
        rc_socket = Mock()
        rc_socket.sendto.side_effect = lambda *_: sent.set()
        status = bridge_status.BridgeStatus()
        with patch.object(control_bridge.evdev, "InputDevice", return_value=dev):
            task = asyncio.create_task(control_bridge.run_pocket("fake-pocket", rc_socket, status))
            try:
                await asyncio.wait_for(sent.wait(), 2)
                self.assertEqual(json.loads(status.packet())["pocket"]["state"], "ready")
                payload, address = rc_socket.sendto.call_args.args
                self.assertEqual(address, ("10.10.10.13", 7002))
                self.assertEqual(control_bridge.HZ, 100)
                self.assertEqual(
                    control_bridge.PACKER.unpack(payload),
                    (
                        192,
                        1792,
                        992,
                        1392,
                        1792,
                        192,
                        1792,
                        192,
                        992,
                        992,
                        992,
                        592,
                        992,
                        992,
                        992,
                        992,
                    ),
                )
            finally:
                task.cancel()
                await asyncio.gather(task, return_exceptions=True)
            self.assertTrue(reader_closed.is_set())
            self.assertEqual(json.loads(status.packet())["pocket"]["state"], "disconnected")
            dev.grab.assert_called_once()
            dev.ungrab.assert_called_once()
            dev.close.assert_called_once()


if __name__ == "__main__":
    unittest.main()
