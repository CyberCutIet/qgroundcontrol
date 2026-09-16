# RPi WFB statistics sender

Reads the local WFB-NG JSON API at TCP 127.0.0.1:9001 and sends compact JSON
datagrams to UDP 127.0.0.1:5800. Uses Python 3 standard library only. No access
to UART, controllers, QGC settings or WFB configuration is required.

The configured WFB `rpi_stats` TX stream 6 carries these datagrams to RADXA,
which forwards them to the laptop at 10.10.10.1:5800. This does not require
the IP tunnel. The sender does not create or modify the WFB stream.

## Manual installation

Copy wfb_rpi_stats.py and wfb-rpi-stats.service to /home/pi on RPi, then run:

```bash
sudo install -D -m 0755 ~/wfb_rpi_stats.py /usr/local/libexec/wfb_rpi_stats.py
sudo install -m 0644 ~/wfb-rpi-stats.service /etc/systemd/system/wfb-rpi-stats.service
sudo systemctl daemon-reload
sudo systemctl enable --now wfb-rpi-stats.service
sudo journalctl -u wfb-rpi-stats.service -n 20 --no-pager
```

Stop forwarding with `sudo systemctl disable --now wfb-rpi-stats.service`.
MAVLink_bridge.py and its service are independent and are not restarted.

## Wire format (version 1)

One UDP datagram contains one RX stream measurement, at most 1400 bytes.
Streams: `MAVLink_uplink rx`, `controller_FPV rx`, `ip_tunnel rx`.
Settings, TX reports and unrelated streams are never forwarded.

- `version`: 1; `source`: `rpi`.
- `session`: random identifier on each API connection; clear receiver history
  when it changes, or when cumulative counters decrease.
- `seq`: sequence number across all streams for that connection. Gaps describe
  missing statistics datagrams, not the underlying control/video packet loss.
- `interval_ms`: API reporting interval, currently 1000. The sender does not
  change RSSI cadence or generate artificial half-second measurements.
- `source_timestamp`: original WFB timestamp, for source ordering/diagnostics;
  do not compare it with laptop wall time (clocks may differ).
- `stream`: original RX stream name.
- `packets`: `out`, `lost`, `fec_rec`, `uniq`, `dec_err`, `bad`, each preserving
  WFB's `[interval_count, cumulative_count]` pair.
- `antennas`: valid receiving antennas, with `ant`, `rssi_avg`, `snr_avg`,
  `pkt_recv`, `freq`, `mcs`, `bw`.
- `has_rssi`: false for reports without valid receiving antenna measurements.

No LQ percentage is invented by the sender. Keep stream histories separate.
The QGC receiver expires each stream independently using local
monotonic arrival time (e.g. after max(3 seconds, 3 report intervals)), shows
missing reception/data explicitly, and rejects duplicate/out-of-order reports.
UDP is best-effort, no acknowledgement/retransmission queue. Local send success
does not prove radio delivery. Cumulative counters allow skipped reports to be
accounted for. Connection loss stops reports until the API reconnects.

The QGC UDP receiver and repeater indicator are implemented in `src/Comms/RpiLinkStatus.*` and `src/Toolbar/RpiRSSIIndicator.qml`.

## Checks

`python3 -m unittest -v test_sender.py` uses local TCP and UDP endpoints to
check framing, reconnect sessions, RSSI transfer, missing reception and invalid
measurements. `systemd-analyze verify wfb-rpi-stats.service` checks unit syntax.
