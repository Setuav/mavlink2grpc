# gRPC API Reference

This page describes the `MavlinkBridge` gRPC service interface, request/response models, and the generic `MavlinkMessage` container.

---

## Service Definition

The core service is defined as `MavlinkBridge` inside the `mavlink2grpc` package.

```protobuf
service MavlinkBridge {
  // Stream MAVLink messages from connected systems
  rpc StreamMessages(StreamFilter) returns (stream MavlinkMessage);

  // Send a MAVLink message to a system
  rpc SendMessage(MavlinkMessage) returns (SendResponse);
}
```

### `StreamMessages`
A server-side streaming RPC. Clients send a `StreamFilter` and the server pushes matching MAVLink messages in real time. If the client disconnects or the stream is closed, the bridge automatically cleans up the subscription.

### `SendMessage`
A unary RPC. Clients populate a `MavlinkMessage` with the desired payload. The bridge serializes this into a raw MAVLink packet and transmits it over the active connection.

---

## Message Schemas

### `StreamFilter`
Filters the incoming stream to reduce network bandwidth and processing overhead.

```protobuf
message StreamFilter {
  // Filter by system ID (0 = receive from all systems)
  uint32 system_id = 1;

  // Filter by component ID (0 = receive from all components)
  uint32 component_id = 2;

  // Filter by message IDs (empty array = receive all messages)
  // E.g., [0, 30, 33] for HEARTBEAT, ATTITUDE, and GLOBAL_POSITION_INT
  repeated uint32 message_ids = 3;
}
```

### `SendResponse`
Indicates whether a command or message was successfully sent to the connection runtime.

```protobuf
message SendResponse {
  // True if message was successfully queued and sent
  bool success = 1;

  // Description of error if success is false
  string error = 2;
}
```

### `MavlinkMessage`
A generic container wrapper that encapsulates metadata and the actual message payload.

```protobuf
message MavlinkMessage {
  // MAVLink sender system ID
  uint32 system_id = 1;

  // MAVLink sender component ID
  uint32 component_id = 2;

  // Unique MAVLink message ID (e.g., 0 for HEARTBEAT)
  uint32 message_id = 3;

  // Microsecond timestamp when packet was processed
  uint64 timestamp_usec = 4;

  // Container holding the specific message type
  oneof payload {
    // Generated automatically: e.g.
    mavlink2grpc.common.Heartbeat heartbeat = 100;
    mavlink2grpc.common.Attitude attitude = 130;
    mavlink2grpc.common.GlobalPositionInt global_position_int = 133;
    // ... all other dialect messages
  }
}
```

!!! note
    Inside the `oneof payload`, field names are formatted in `snake_case` (matching MAVLink message names in lowercase), and target types are under the `mavlink2grpc.[dialect]` package.
