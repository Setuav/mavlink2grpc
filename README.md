![MAVLink2gRPC Banner](docs/images/mavlink2grpc_banner.png)

[![CI](https://github.com/Setuav/mavlink2grpc/actions/workflows/ci.yml/badge.svg)](https://github.com/Setuav/mavlink2grpc/actions/workflows/ci.yml)
[![Docs](https://github.com/Setuav/mavlink2grpc/actions/workflows/docs.yml/badge.svg)](https://github.com/Setuav/mavlink2grpc/actions/workflows/docs.yml)
[![Release](https://github.com/Setuav/mavlink2grpc/actions/workflows/release.yml/badge.svg)](https://github.com/Setuav/mavlink2grpc/actions/workflows/release.yml)
[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-blue)](https://setuav.github.io/mavlink2grpc/)
[![PyPI](https://img.shields.io/pypi/v/mavlink2grpc-proto)](https://pypi.org/project/mavlink2grpc-proto/)
[![npm](https://img.shields.io/npm/v/%40setuav%2Fmavlink2grpc-proto)](https://www.npmjs.com/package/@setuav/mavlink2grpc-proto)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

## About

A high-performance, native bridge and code generator that exposes the entire MAVLink protocol as type-safe gRPC services using modern C++17.

Designed as a robust, type-safe alternative to [mavlink2rest](https://github.com/mavlink/mavlink2rest) for professional UAV systems.

## Documentation

The full documentation is available at:

**https://setuav.github.io/mavlink2grpc/**

Key sections:

- [Getting Started](https://setuav.github.io/mavlink2grpc/getting_started/)
- [Architecture & Design](https://setuav.github.io/mavlink2grpc/architecture/)
- [gRPC API Reference](https://setuav.github.io/mavlink2grpc/api_reference/)
- [Client Libraries](https://setuav.github.io/mavlink2grpc/client_libraries/)
- [Developer & Testing Guide](https://setuav.github.io/mavlink2grpc/developer_guide/)

## Packages

Python gRPC stubs:

```bash
pip install mavlink2grpc-proto
```

Node.js Protocol Buffer definitions:

```bash
npm install @setuav/mavlink2grpc-proto
```

## Features
* **Schema-First:** Auto-generates Protobuf definitions (`.proto`) directly from MAVLink XMLs.
* **High Performance:** C++17 runtime bridge designed for low latency and efficient resource usage.
* **Polyglot:** Consume MAVLink telemetry in Python, Go, Rust, or Web clients via gRPC.
* **Real-time Streaming:** Bidirectional MAVLink message streaming with gRPC server-side streaming.
* **Web Inspector:** Built-in QGroundControl-style MAVLink inspector with live charts and frequency tracking.

## Project Structure

### `generator/`
Python-based code generator that converts MAVLink XML definitions to Protocol Buffer files and C++ conversion code. Uses Jinja2 templates to generate `.proto` files, gRPC service definitions, and C++ MAVLink↔Protobuf converters from XML message definitions.

### `bridge/`
C++17 MAVLink-to-gRPC bridge that connects to MAVLink devices (UDP/Serial) and exposes real-time bidirectional message streaming via gRPC. Implements connection management, message routing, and gRPC service (`StreamMessages`, `SendMessage`). Supports MAVSDK-style connection URLs like `udp://:14550` or `serial:///dev/ttyUSB0:57600`.

### `examples/inspector/`
QGroundControl-style web-based MAVLink inspector. Node.js backend (Express + Socket.IO) acts as gRPC client to the bridge and WebSocket server for the browser. Vanilla JavaScript frontend provides real-time message monitoring, frequency tracking, and multi-field chart visualization with Chart.js.

## Installation

### Requirements

- **C++:** C++17 compatible compiler, CMake 3.10+, gRPC, Protocol Buffers
- **Python:** 3.8+
- **Node.js:** 20+

### Setup
```bash
# Clone repository
git clone https://github.com/Setuav/mavlink2grpc.git
cd mavlink2grpc

# Run setup script (clones mavlink submodule, installs dependencies)
./setup.sh

# Build bridge (automatically runs the python generator and compiles the project)
mkdir build && cd build
cmake .. -DMAVLINK_DIALECT=common
make -j$(nproc)

# Install inspector dependencies
cd ../examples/inspector
npm install
```

## Usage

Start px4 or any MAVLink-compatible autopilot simulator (e.g., `px4_sitl`).

### Start Bridge
```bash
./build/bridge/mav2grpc_bridge -c udp://:14550 -g 0.0.0.0:50051
# Change connection string (-c) as needed
```

### Start Inspector Demo
```bash
cd examples/inspector
node server.js -g localhost:50051 -p 8000
```

Open browser: `http://localhost:8000`