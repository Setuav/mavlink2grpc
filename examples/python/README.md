# Examples

Python examples demonstrating how to use the mavlink2grpc bridge.

## Prerequisites

```bash
# Install Python gRPC dependencies
pip install grpcio grpcio-tools

# Create python files from generated protos
python3 -m grpc_tools.protoc -I./proto --python_out=./generated --grpc_python_out=./generated ./proto/mavlink_bridge.proto ./proto/mavlink/common.proto
```

## Available Examples

### stream_telemetry.py

Subscribes to real-time MAVLink telemetry streams (HEARTBEAT, ATTITUDE, and GLOBAL_POSITION_INT) and prints updates as they arrive.

**Usage:**
```bash
python3 stream_telemetry.py --host localhost:50051
```

### takeoff.py

Sends guided takeoff commands (set GUIDED mode, arm vehicle, send takeoff command) to a MAVLink vehicle and monitors for acknowledgment messages.

**Usage:**
```bash
# Takeoff to 10 meters (default)
python3 takeoff.py

# Custom altitude
python3 takeoff.py --altitude 20.0
```

## Running the Examples

1. Make sure the bridge is running:
```bash
cd ../bridge/build
./mav2grpc_bridge -c udp://:14550 -g 0.0.0.0:50051
```

2. Connect your MAVLink vehicle (e.g., SITL, real drone, or simulator)

3. Run the example:
```bash
cd ../examples/python
python3 takeoff.py --altitude 10.0
```

## Creating Your Own Client

You can use these examples as a starting point for your own MAVLink client. The general pattern is:

1. Import generated proto files from `../generated/`
2. Create a gRPC channel and stub
3. Create MAVLink messages using the proto message types
4. Send messages using `stub.SendMessage()`
5. Stream messages using `stub.StreamMessages()`

See `takeoff.py` for a complete working example.
