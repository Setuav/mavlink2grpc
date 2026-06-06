#!/usr/bin/env python3
"""
Example: Real-time Telemetry Streaming

This script demonstrates how to subscribe to specific MAVLink message streams
(e.g., HEARTBEAT, ATTITUDE, GLOBAL_POSITION_INT) and log their fields in real time.
"""

import grpc
import sys
import argparse
from pathlib import Path

# Add generated proto files to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "generated"))

import mavlink_bridge_pb2
import mavlink_bridge_pb2_grpc
from mavlink import common_pb2


def main():
    parser = argparse.ArgumentParser(description="MAVLink Telemetry Streaming Client")
    parser.add_argument(
        "--host",
        type=str,
        default="localhost:50051",
        help="gRPC server host (default: localhost:50051)"
    )
    args = parser.parse_args()

    # 1. Connect to gRPC channel
    print(f"Connecting to gRPC bridge at {args.host}...")
    channel = grpc.insecure_channel(args.host)
    stub = mavlink_bridge_pb2_grpc.MavlinkBridgeStub(channel)

    # 2. Define message filters
    # We filter by message IDs:
    #   0  = HEARTBEAT
    #   30 = ATTITUDE
    #   33 = GLOBAL_POSITION_INT
    stream_filter = mavlink_bridge_pb2.StreamFilter(
        system_id=0,       # 0 means listen to all systems
        component_id=0,    # 0 means listen to all components
        message_ids=[0, 30, 33]
    )

    print("Subscribing to telemetry streams (HEARTBEAT, ATTITUDE, GLOBAL_POSITION_INT)...")
    print("Press Ctrl+C to exit.\n")

    try:
        # 3. Stream messages
        for msg in stub.StreamMessages(stream_filter):
            sys_id = msg.system_id
            comp_id = msg.component_id

            if msg.HasField("heartbeat"):
                hb = msg.heartbeat
                # Look up base mode / system status names if enum helpers exist
                mode = hb.base_mode
                state = hb.system_status
                try:
                    mode = common_pb2.MavModeFlag.Name(hb.base_mode)
                except (ValueError, AttributeError):
                    pass
                try:
                    state = common_pb2.MavState.Name(hb.system_status)
                except (ValueError, AttributeError):
                    pass
                print(f"[SYS {sys_id}] HEARTBEAT | Base Mode: {mode} | Status: {state}")

            elif msg.HasField("attitude"):
                att = msg.attitude
                print(f"[SYS {sys_id}] ATTITUDE  | Roll: {att.roll:.4f} rad | Pitch: {att.pitch:.4f} rad | Yaw: {att.yaw:.4f} rad")

            elif msg.HasField("global_position_int"):
                pos = msg.global_position_int
                lat = pos.lat / 1e7
                lon = pos.lon / 1e7
                alt = pos.alt / 1000.0  # MSL
                print(f"[SYS {sys_id}] GPS       | Lat: {lat:.7f} | Lon: {lon:.7f} | Alt: {alt:.2f}m")

    except grpc.RpcError as e:
        print(f"\nRPC Error occurred: {e.code()} - {e.details()}")
    except KeyboardInterrupt:
        print("\nExiting telemetry client.")


if __name__ == "__main__":
    main()
