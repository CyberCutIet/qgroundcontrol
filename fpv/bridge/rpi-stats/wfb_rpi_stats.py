#!/usr/bin/env python3
"""Forward per-stream RPi WFB RX measurements to a dedicated UDP radio stream."""

import argparse
import json
import logging
import math
import signal
import socket
import threading
import time
import uuid

STREAMS = {"MAVLink_uplink rx", "controller_FPV rx", "ip_tunnel rx"}
COUNTERS = ("out", "lost", "fec_rec", "uniq", "dec_err", "bad")
ANT_FIELDS = ("ant", "rssi_avg", "snr_avg", "pkt_recv", "freq", "mcs", "bw")
MAX_LINE = 65536
MAX_DATAGRAM = 1400


def number(value):
    return type(value) in (int, float) and math.isfinite(value)


def measurement(message):
    """Whitelist measurements; never transmit the API's settings/keys."""
    if not isinstance(message, dict):
        return None
    if message.get("type") != "rx" or message.get("id") not in STREAMS:
        return None
    packets = message.get("packets")
    antennas = message.get("rx_ant_stats")
    timestamp = message.get("timestamp")
    if not isinstance(packets, dict) or not isinstance(antennas, list) or not number(timestamp):
        return None
    clean = {}
    for key in COUNTERS:
        pair = packets.get(key)
        if (not isinstance(pair, list) or len(pair) != 2
                or not all(number(v) and v >= 0 and int(v) == v for v in pair)
                or pair[1] < pair[0]):
            return None
        clean[key] = pair
    ant_values = []
    for ant in antennas:
        if not isinstance(ant, dict) or not all(number(ant.get(k)) for k in ANT_FIELDS):
            return None
        if ant["pkt_recv"] > 0 and -127 <= ant["rssi_avg"] < 0:
            ant_values.append({key: ant[key] for key in ANT_FIELDS})
    return {"stream": message["id"], "source_timestamp": timestamp,
            "packets": clean, "antennas": ant_values,
            "has_rssi": bool(ant_values)}


def run(args, stop):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
        udp.setblocking(False)
        while not stop.is_set():
            try:
                with socket.create_connection((args.api_host, args.api_port), timeout=3) as api:
                    api.settimeout(1)
                    logging.info("WFB API connected")
                    session = uuid.uuid4().hex
                    sequence = 0
                    interval = 1000
                    last_report = time.monotonic()
                    last_timestamp = {}
                    buffer = b""
                    while not stop.is_set():
                        if time.monotonic() - last_report > max(5, interval / 1000 * 3):
                            raise TimeoutError("No valid RX reports")
                        try:
                            data = api.recv(65536)
                        except socket.timeout:
                            continue
                        if not data:
                            raise ConnectionError("WFB API closed")
                        buffer += data
                        if len(buffer) > MAX_LINE * 2:
                            raise ValueError("API buffer too large")
                        # Keep the newest report per stream from this read, not a UDP backlog.
                        pending = {}
                        while b"\n" in buffer:
                            line, buffer = buffer.split(b"\n", 1)
                            if len(line) > MAX_LINE:
                                raise ValueError("API line too large")
                            try:
                                message = json.loads(line)
                            except (ValueError, UnicodeError):
                                continue
                            if isinstance(message, dict) and message.get("type") == "settings":
                                settings = message.get("settings", {})
                                common = settings.get("common", {}) if isinstance(settings, dict) else {}
                                value = common.get("log_interval") if isinstance(common, dict) else None
                                if type(value) is int and 100 <= value <= 5000:
                                    interval = value
                                continue
                            report = measurement(message)
                            if report is not None:
                                pending[report["stream"]] = report
                                last_report = time.monotonic()
                        if len(buffer) > MAX_LINE:
                            raise ValueError("API line too large")
                        for stream, report in pending.items():
                            timestamp = report["source_timestamp"]
                            if timestamp == last_timestamp.get(stream):
                                continue
                            last_timestamp[stream] = timestamp
                            sequence += 1
                            report.update(version=1, source="rpi", session=session,
                                          seq=sequence, interval_ms=interval)
                            payload = json.dumps(report, separators=(",", ":"), allow_nan=False).encode()
                            if len(payload) > MAX_DATAGRAM:
                                logging.warning("Skipping oversized report for %s", stream)
                                continue
                            try:
                                udp.sendto(payload, (args.udp_host, args.udp_port))
                            except OSError:
                                # No retries/queue: the next report carries cumulative counters.
                                logging.warning("UDP report dropped locally")
            except (OSError, ValueError) as exc:
                logging.warning("WFB API unavailable: %s", exc)
                stop.wait(2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--api-host", default="127.0.0.1")
    parser.add_argument("--api-port", type=int, default=9001)
    parser.add_argument("--udp-host", default="127.0.0.1")
    parser.add_argument("--udp-port", type=int, default=5800)
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
    stop = threading.Event()
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: stop.set())
    run(args, stop)


if __name__ == "__main__":
    main()
