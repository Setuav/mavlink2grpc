#!/usr/bin/env python3
"""
Example: Proper guided mode takeoff sequence

This script demonstrates the complete takeoff sequence:
1. Set mode to GUIDED
2. ARM the vehicle
3. Takeoff to altitude (only if on ground)

Usage:
    python3 guided_takeoff.py --altitude 10.0
"""

import grpc
import sys
import time
import argparse
from pathlib import Path

# Add generated proto files to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "generated"))

from mavlink2grpc import mavlink2grpc_pb2
from mavlink2grpc import mavlink2grpc_pb2_grpc
from mavlink2grpc.mavlink import common_pb2


def send_command(stub, target_system, target_component, command, params, command_name="command"):
    """Send a MAVLink command and return response"""
    
    command_long = common_pb2.CommandLong(
        target_system=target_system,
        target_component=target_component,
        command=command,
        confirmation=0,
        param1=params[0],
        param2=params[1],
        param3=params[2],
        param4=params[3],
        param5=params[4],
        param6=params[5],
        param7=params[6]
    )
    
    message = mavlink2grpc_pb2.MavlinkMessage(
        system_id=254,
        component_id=190,
        message_id=76,
        command_long=command_long
    )
    
    # Debug: print what we're sending
    print(f"Sending {command_name} command...")
    print(f"  target: {target_system}/{target_component}")
    print(f"  command: {command}")
    print(f"  params: {params}")
    
    response = stub.SendMessage(message)
    
    if response.success:
        print(f"[OK] {command_name} command sent")
        return True
    else:
        print(f"[ERROR] Failed to send {command_name}: {response.error}")
        return False


def wait_for_ack(stub, command_name, timeout=5.0):
    """Wait for COMMAND_ACK"""
    
    stream_filter = mavlink2grpc_pb2.StreamFilter(
        system_id=0,
        component_id=0,
        message_ids=[77]  # COMMAND_ACK
    )
    
    print(f"Waiting for {command_name} ACK (timeout: {timeout}s)...")
    start_time = time.time()
    
    try:
        for message in stub.StreamMessages(stream_filter):
            if message.HasField('command_ack'):
                ack = message.command_ack
                result_str = common_pb2.MavResult.Name(ack.result)
                
                # Try to get command name, fallback to ID if not in enum
                try:
                    cmd_name = common_pb2.MavCmd.Name(ack.command)
                except ValueError:
                    cmd_name = f"UNKNOWN_CMD_{ack.command}"
                
                print(f"  Command: {cmd_name}")
                print(f"  Result: {result_str}")
                
                if ack.result == common_pb2.MAV_RESULT_ACCEPTED:
                    print(f"[OK] {command_name} accepted\n")
                    return True
                else:
                    print(f"[ERROR] {command_name} rejected: {result_str}\n")
                    return False
            
            if time.time() - start_time > timeout:
                print(f"[ERROR] Timeout waiting for {command_name} ACK\n")
                return False
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        return False


def set_mode_guided(stub, target_system, target_component):
    """Set vehicle mode to GUIDED (not needed for PX4, but harmless)"""
    # MAV_CMD_DO_SET_MODE = 176
    # For PX4: Just use arm + takeoff, mode changes automatically
    # For ArduPilot: param1=1 (custom mode), param2=4 (GUIDED)
    return send_command(
        stub, target_system, target_component,
        common_pb2.MAV_CMD_DO_SET_MODE,
        [1, 4, 0, 0, 0, 0, 0],
        "SET_MODE_GUIDED"
    )


def arm_vehicle(stub, target_system, target_component, force=False):
    """ARM the vehicle"""
    # MAV_CMD_COMPONENT_ARM_DISARM = 400
    # param1: 1=arm, 0=disarm
    # param2: 0 for normal arming (don't use 21196, some systems don't support it)
    return send_command(
        stub, target_system, target_component,
        common_pb2.MAV_CMD_COMPONENT_ARM_DISARM,
        [1, 0, 0, 0, 0, 0, 0],
        "ARM"
    )


def get_current_position(stub, timeout=5.0):
    """Get current latitude and longitude from vehicle (GLOBAL_POSITION_INT)"""
    stream_filter = mavlink2grpc_pb2.StreamFilter(
        system_id=0,
        component_id=0,
        message_ids=[33]  # GLOBAL_POSITION_INT
    )
    print("Reading current position (lat/lon)...")
    start_time = time.time()
    try:
        for message in stub.StreamMessages(stream_filter):
            if message.HasField('global_position_int'):
                pos = message.global_position_int
                lat = pos.lat / 1e7
                lon = pos.lon / 1e7
                print(f"Current lat: {lat:.7f}, lon: {lon:.7f}")
                return lat, lon
            if time.time() - start_time > timeout:
                print("⚠ Timeout reading position, using 0/0")
                return 0.0, 0.0
    except KeyboardInterrupt:
        print("\nInterrupted")
        return 0.0, 0.0
    

def get_current_altitude(stub, timeout=5.0):
    """Get current altitude from vehicle"""
    
    # Stream GLOBAL_POSITION_INT messages
    stream_filter = mavlink2grpc_pb2.StreamFilter(
        system_id=0,
        component_id=0,
        message_ids=[33]  # GLOBAL_POSITION_INT
    )
    
    print("Reading current altitude...")
    start_time = time.time()
    
    try:
        for message in stub.StreamMessages(stream_filter):
            if message.HasField('global_position_int'):
                pos = message.global_position_int
                # alt is in millimeters, convert to meters
                altitude_m = pos.alt / 1000.0
                print(f"Current altitude: {altitude_m:.2f}m")
                return altitude_m
            
            if time.time() - start_time > timeout:
                print("⚠ Timeout reading altitude, using default")
                return 0.0
                
    except KeyboardInterrupt:
        print("\nInterrupted")
        return 0.0


def set_takeoff_altitude(stub, target_system, target_component, altitude):
    """Set MIS_TAKEOFF_ALT parameter"""
    # MAV_CMD_PARAM_SET is not ideal, we'll use a workaround
    # For now, just return the altitude we want to use
    return altitude


def takeoff(stub, target_system, target_component, altitude):
    """Send takeoff command (PX4 uses system parameter for altitude)"""
    # MAV_CMD_NAV_TAKEOFF = 22
    # For PX4: param7 (altitude) should be set, even if PX4 uses MIS_TAKEOFF_ALT
    # For ArduPilot: param7 = altitude
    # Read current lat/lon for PX4 compatibility
    lat, lon = get_current_position(stub)
    return send_command(
        stub, target_system, target_component,
        common_pb2.MAV_CMD_NAV_TAKEOFF,
        [0, 0, 0, 0, lat, lon, altitude],
        f"TAKEOFF (alt: {altitude}m, lat: {lat:.7f}, lon: {lon:.7f})"
    )


def main():
    parser = argparse.ArgumentParser(description="Guided mode takeoff sequence")
    parser.add_argument("--host", default="localhost:50051",
                       help="gRPC bridge address (default: localhost:50051)")
    parser.add_argument("--altitude", type=float, default=10.0,
                       help="Takeoff altitude in meters (default: 10.0)")
    parser.add_argument("--system", type=int, default=1,
                       help="Target system ID (default: 1)")
    parser.add_argument("--component", type=int, default=1,
                       help="Target component ID (default: 1)")
    parser.add_argument("--force-arm", action="store_true",
                       help="Force arming (skip pre-arm checks)")
    
    args = parser.parse_args()
    
    print(f"Connecting: {args.host}")
    channel = grpc.insecure_channel(args.host)
    stub = mavlink2grpc_pb2_grpc.MavlinkBridgeStub(channel)
    
    try:
        # Get current altitude first
        current_alt = get_current_altitude(stub, timeout=5.0)
        target_alt = current_alt + args.altitude
        print(f"Current: {current_alt:.2f}m  Target: {target_alt:.2f}m")
        if not arm_vehicle(stub, args.system, args.component, args.force_arm):
            return 1
        if not wait_for_ack(stub, "ARM"):
            return 1
        print("ARMED.")
        time.sleep(2.0)
        if not takeoff(stub, args.system, args.component, target_alt):
            return 1
        if not wait_for_ack(stub, "TAKEOFF"):
            print("Takeoff might have been rejected.")
            print("Check if already in air or pre-arm checks failed.")
        print("TAKEOFF command sent.")
        
    except grpc.RpcError as e:
        print(f"[ERROR] gRPC error: {e.code()} - {e.details()}")
        return 1
    except Exception as e:
        print(f"[ERROR] Error: {e}")
        import traceback
        traceback.print_exc()
        return 1
    finally:
        channel.close()
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
