"""Read-only QGC status, independent of the 100 Hz RC packet sender."""

import asyncio
import json
import socket
from contextlib import suppress

STATUS_ADDRESS = ("127.0.0.1", 17002)
STATUS_INTERVAL = 0.2


class BridgeStatus:
    def __init__(self):
        self.set("disconnected")

    def set(self, state, error=""):
        self._pocket = {"state": state, "error": error}

    def packet(self):
        return json.dumps(
            {"v": 1, "type": "qgc_control_bridge", "pocket": self._pocket},
            separators=(",", ":"),
        ).encode("utf-8")

    async def publish(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.setblocking(False)
            while True:
                # QGC status must never interrupt or delay RC forwarding.
                with suppress(OSError):
                    sock.sendto(self.packet(), STATUS_ADDRESS)
                await asyncio.sleep(STATUS_INTERVAL)
