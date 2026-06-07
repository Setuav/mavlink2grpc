# MAVLink2gRPC Python Client Stubs

This package contains the auto-generated gRPC and Protocol Buffer client stubs for communicating with a `MAVLink2gRPC` bridge server.

## Installation

```bash
pip install mavlink2grpc-proto
```

## Usage

```python
import grpc
import mavlink_bridge_pb2
import mavlink_bridge_pb2_grpc
from mavlink import common_pb2

# Connect to the gRPC bridge
channel = grpc.insecure_channel('localhost:50051')
stub = mavlink_bridge_pb2_grpc.MavlinkBridgeStub(channel)

# Subscribe to HEARTBEAT messages
stream_filter = mavlink_bridge_pb2.StreamFilter(
    system_id=0,
    component_id=0,
    message_ids=[0] # HEARTBEAT message ID is 0
)

for message in stub.StreamMessages(stream_filter):
    if message.has_heartbeat:
        print(f"Received HEARTBEAT: Type {message.heartbeat.type}")
```
