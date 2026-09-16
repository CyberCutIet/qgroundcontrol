import argparse
import json
import socket
import threading
import unittest

import wfb_rpi_stats as sender


def sample():
    return {"type": "rx", "id": "MAVLink_uplink rx", "timestamp": 1234,
            "packets": {k: [0, 0] for k in sender.COUNTERS}, "rx_ant_stats": []}


class SenderTest(unittest.TestCase):
    def test_no_reception_and_invalid_values(self):
        report = sender.measurement(sample())
        self.assertFalse(report["has_rssi"])
        self.assertEqual(report["antennas"], [])
        bad = sample()
        bad["packets"]["lost"] = [-1, 0]
        self.assertIsNone(sender.measurement(bad))
        bad["packets"]["lost"] = [float("nan"), 0]
        self.assertIsNone(sender.measurement(bad))

    def test_whitelist(self):
        self.assertIsNone(sender.measurement({"type": "settings", "secret": "never-send"}))
        message = sample()
        message["id"] = "rpi_stats tx"
        self.assertIsNone(sender.measurement(message))

    def test_tcp_fragmentation_udp_and_reconnect(self):
        stop = threading.Event()
        with socket.socket() as server, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
            server.bind(("127.0.0.1", 0))
            server.listen()
            server.settimeout(6)
            udp.bind(("127.0.0.1", 0))
            udp.settimeout(6)
            args = argparse.Namespace(api_host="127.0.0.1", api_port=server.getsockname()[1],
                                      udp_host="127.0.0.1", udp_port=udp.getsockname()[1])
            worker = threading.Thread(target=sender.run, args=(args, stop))
            worker.start()
            try:
                sessions = []
                for _ in range(2):
                    conn, _ = server.accept()
                    with conn:
                        conn.sendall(b'{"type":"settings","secret":"never-send"}\ninvalid\n')
                        message = sample()
                        message["rx_ant_stats"] = [dict(zip(sender.ANT_FIELDS, [1, -63, 19, 10, 5200, 1, 40]))]
                        wire = json.dumps(message).encode() + b"\n"
                        conn.sendall(wire[:30])
                        conn.sendall(wire[30:])
                        data, _ = udp.recvfrom(2048)
                        report = json.loads(data)
                        self.assertNotIn(b"never-send", data)
                        self.assertEqual(report["antennas"][0]["rssi_avg"], -63)
                        self.assertEqual(report["seq"], 1)
                        self.assertLessEqual(len(data), sender.MAX_DATAGRAM)
                        sessions.append(report["session"])
                self.assertNotEqual(*sessions)
            finally:
                stop.set()
                worker.join(6)
                self.assertFalse(worker.is_alive())


if __name__ == "__main__":
    unittest.main()
