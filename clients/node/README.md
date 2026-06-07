# MAVLink2gRPC Node.js Client Definitions

This package contains the gRPC Protocol Buffer definition files for communicating with a `MAVLink2gRPC` bridge server.

## Installation

```bash
npm install mavlink2grpc-proto
```

## Usage

```javascript
const grpc = require('@grpc/grpc-js');
const protoLoader = require('@grpc/proto-loader');
const { protoPath, protoDir } = require('mavlink2grpc-proto');

// Load proto definitions
const packageDefinition = protoLoader.loadSync(protoPath, {
    keepCase: true,
    longs: String,
    enums: String,
    defaults: true,
    oneofs: true,
    includeDirs: [protoDir]
});

const proto = grpc.loadPackageDefinition(packageDefinition);

// Connect to the gRPC bridge
const client = new proto.mavlink.MavlinkBridge(
    'localhost:50051',
    grpc.credentials.createInsecure()
);
```
