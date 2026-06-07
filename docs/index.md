# MAVLink2gRPC

![MAVLink2gRPC Banner](images/mavlink2grpc_banner.png)

Welcome to the official documentation for **MAVLink2gRPC** — a high-performance, native bridge and code generator that exposes the entire MAVLink protocol as type-safe gRPC services using modern C++17.

MAVLink2gRPC is designed as a robust, type-safe alternative to REST-based bridges for professional UAV and robotics systems where low latency, low resource footprint, and compile-time API safety are critical.

---

## Why MAVLink2gRPC?

While traditional MAVLink APIs rely on REST (like `mavlink2rest`) or complex client-side SDKs, MAVLink2gRPC leverages **gRPC** and **Protocol Buffers** to deliver:

*   **Schema-First Design:** Entire MAVLink message sets are auto-generated into standard Protobuf files (`.proto`), providing immediate type safety and auto-completion in all supported languages.
*   **High Performance:** Built using native C++17, delivering sub-millisecond bridge latency with extremely low CPU and memory footprints.
*   **Polyglot ecosystem:** Build your ground station, telemetry logger, or cloud connector in Python, Go, Node.js, C++, Rust, or C# using standard gRPC code generation.
*   **Bidirectional Streaming:** Efficient real-time telemetry streaming and command execution via gRPC server-streaming RPCs.

---

## Key Documentation Sections

To help you get started quickly:

*   [**Getting Started**](getting_started.md): System requirements, build instructions, and running the bridge daemon.
*   [**Architecture & Design**](architecture.md): Deep-dive into the code generator, the C++ message router, and data flow.
*   [**gRPC API Reference**](api_reference.md): Explanation of `MavlinkBridge` service definitions, filters, and payload oneofs.
*   [**Client Libraries**](client_libraries.md): Importing and consuming the auto-generated stubs in Node.js and Python.
*   [**Developer Guide**](developer_guide.md): Modifying generators, adding custom dialects, and running tests.
