# Client Libraries

MAVLink2gRPC provides pre-compiled client packages for both **Node.js** and **Python**, containing ready-to-use gRPC stubs and Protobuf definitions.

---

## Node.js Client (`@setuav/mavlink2grpc-proto`)

The Node.js package exports the raw `.proto` definitions and absolute file paths so you can load them dynamically with `@grpc/proto-loader`.

### Installation

```bash
npm install @setuav/mavlink2grpc-proto @grpc/grpc-js @grpc/proto-loader
```

### Usage Example

```javascript
const path = require('path');
const grpc = require('@grpc/grpc-js');
const protoLoader = require('@grpc/proto-loader');
const { protoPath, protoDir } = require('@setuav/mavlink2grpc-proto');

// Load proto definitions dynamically
const packageDefinition = protoLoader.loadSync(protoPath, {
    keepCase: true,
    longs: String,
    enums: String,
    defaults: true,
    oneofs: true,
    includeDirs: [protoDir] // Crucial: contains dialect dependencies
});

const proto = grpc.loadPackageDefinition(packageDefinition);

// Create gRPC client
const client = new proto.mavlink2grpc.MavlinkBridge(
    'localhost:50051',
    grpc.credentials.createInsecure()
);

// Stream heartbeat messages
const stream = client.StreamMessages({
    system_id: 0,
    component_id: 0,
    message_ids: [0] // HEARTBEAT
});

stream.on('data', (msg) => {
    console.log(`Received packet from system ${msg.system_id}`);
    if (msg.heartbeat) {
        console.log(`Autopilot Type: ${msg.heartbeat.autopilot}`);
    }
});
```

---

## Python Client (`mavlink2grpc-proto`)

The Python package distributes pre-compiled Python stubs (`_pb2` and `_pb2_grpc` files) directly matching the `mavlink2grpc` namespace.

### Installation

```bash
pip install mavlink2grpc-proto grpcio
```

### Usage Example

```python
import grpc
from mavlink2grpc import mavlink2grpc_pb2
from mavlink2grpc import mavlink2grpc_pb2_grpc
from mavlink2grpc.mavlink import common_pb2

def run():
    # Connect to gRPC server
    channel = grpc.insecure_channel('localhost:50051')
    stub = mavlink2grpc_pb2_grpc.MavlinkBridgeStub(channel)

    # Configure filter (subscribe to HEARTBEAT and GLOBAL_POSITION_INT)
    stream_filter = mavlink2grpc_pb2.StreamFilter(
        system_id=0,
        component_id=0,
        message_ids=[0, 33]
    )

    try:
        for message in stub.StreamMessages(stream_filter):
            if message.HasField('heartbeat'):
                print(f"[Heartbeat] Autopilot: {message.heartbeat.autopilot}")
            elif message.HasField('global_position_int'):
                pos = message.global_position_int
                print(f"[GPS] Lat: {pos.lat/1e7}, Lon: {pos.lon/1e7}, Alt: {pos.alt/1000}m")
    except grpc.RpcError as e:
        print(f"gRPC stream connection failed: {e.details()}")

if __name__ == '__main__':
    run()
```
