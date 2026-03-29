#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Send sample protocol v1 JSON datagrams to the 3D Spatial plugin UDP listener (127.0.0.1:9876)."""

import json
import socket
import sys
import time

HOST = "127.0.0.1"
UDP_PORT = 9876


def build_test_message(event_type):
    if event_type == "damage_event":
        return {
            "version": 1,
            "type": "damage_event",
            "timestamp_ms": int(time.time() * 1000),
            "source": "udp-test",
            "amount": 15.0,
            "damage_type": "generic",
            "dir_x": -0.5,
            "dir_y": 0.0,
            "dir_z": 0.5,
        }

    if event_type == "health_state":
        return {
            "version": 1,
            "type": "health_state",
            "timestamp_ms": int(time.time() * 1000),
            "source": "udp-test",
            "health": 42.0,
            "health_max": 100.0,
        }

    if event_type == "world_light":
        return {
            "version": 1,
            "type": "world_light",
            "timestamp_ms": int(time.time() * 1000),
            "source": "udp-test",
            "x": 1.0,
            "y": 2.2,
            "z": 0.5,
            "r": 255,
            "g": 180,
            "b": 120,
            "intensity": 1.0,
        }

    return {
        "version": 1,
        "type": "player_pose",
        "timestamp_ms": int(time.time() * 1000),
        "source": "udp-test",
        "x": 0.0,
        "y": 1.7,
        "z": 0.0,
        "fx": 0.0,
        "fy": 0.0,
        "fz": 1.0,
        "ux": 0.0,
        "uy": 1.0,
        "uz": 0.0,
    }


def send_udp(host, port, event_type, burst, stream_hz=0.0, stream_seconds=0.0):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        if stream_hz > 0.0 and stream_seconds > 0.0:
            interval = 1.0 / stream_hz
            end_time = time.time() + stream_seconds
            sent = 0
            while time.time() < end_time:
                msg = build_test_message(event_type)
                payload = json.dumps(msg, separators=(",", ":")).encode("utf-8")
                sock.sendto(payload, (host, port))
                sent += 1
                time.sleep(interval)
            print(
                f"Streamed {sent} '{event_type}' packet(s) via UDP to {host}:{port} "
                f"at ~{stream_hz:.1f} Hz for {stream_seconds:.1f}s"
            )
            return

        for _ in range(burst):
            msg = build_test_message(event_type)
            payload = json.dumps(msg, separators=(",", ":")).encode("utf-8")
            sock.sendto(payload, (host, port))
    print(f"Sent {burst} '{event_type}' packet(s) via UDP to {host}:{port}")


def parse_args():
    host = HOST
    port = UDP_PORT
    event_type = "player_pose"
    burst = 1
    stream_hz = 0.0
    stream_seconds = 5.0

    args = sys.argv[1:]
    if args and args[0] == "--udp":
        args = args[1:]

    i = 0
    while i < len(args):
        arg = args[i]
        if arg == "--type" and (i + 1) < len(args):
            event_type = args[i + 1]
            i += 2
            continue
        if arg == "--burst" and (i + 1) < len(args):
            burst = max(1, int(args[i + 1]))
            i += 2
            continue
        if arg == "--stream":
            if (i + 1) >= len(args):
                raise ValueError("--stream expects at least <hz>")
            stream_hz = max(0.0, float(args[i + 1]))
            if (i + 2) < len(args) and not args[i + 2].startswith("--"):
                stream_seconds = max(0.0, float(args[i + 2]))
                i += 3
            else:
                i += 2
            continue
        if host == HOST:
            host = arg
        elif port == UDP_PORT:
            port = int(arg)
        i += 1

    return host, port, event_type, burst, stream_hz, stream_seconds


def main():
    host, port, event_type, burst, stream_hz, stream_seconds = parse_args()
    try:
        send_udp(host, port, event_type, burst, stream_hz, stream_seconds)
    except Exception as ex:
        print(f"Failed to send test packet: {ex}")
        print("Examples:")
        print("  python tools/game_telemetry_udp_test_sender.py --type player_pose --stream 60 10")
        print("  python tools/game_telemetry_udp_test_sender.py 127.0.0.1 9876")
        sys.exit(1)


if __name__ == "__main__":
    main()
