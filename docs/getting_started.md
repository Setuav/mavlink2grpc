# Getting Started

This guide walks you through system requirements, installation, building the C++ bridge, and running your first MAVLink-to-gRPC pipeline.

---

## Prerequisites

MAVLink2gRPC requires a Linux-based environment (fully tested on **Ubuntu 22.04 / 24.04**).

### System Dependencies

You need to install standard build tools, CMake, Python 3, Node.js, and gRPC development libraries.

On Ubuntu/Debian systems, you can install them using:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc \
    nlohmann-json3-dev
```

---

## Installation & Build

### 1. Clone the Repository

Clone the project and initialize the standard MAVLink XML definitions submodule:

```bash
git clone https://github.com/Setuav/mavlink2grpc.git
cd mavlink2grpc
```

### 2. Setup Environment

Run the setup helper script to ensure the MAVLink submodule is fully updated and python package requirements are installed:

```bash
./setup.sh
```

### 3. Build the Bridge

Create a build directory, run CMake, and compile. 

!!! tip
    You can choose which MAVLink XML dialect to compile by passing `-DMAVLINK_DIALECT=<dialect>`. Supported dialects include `minimal`, `standard`, and `common` (default).

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DMAVLINK_DIALECT=common
make -j$(nproc)
```

The build process automatically runs the Python code generator at configure-time, producing the `.proto` schemas and the C++ conversion stubs before compiling the executable.

---

## Running the Bridge

Once the build finishes successfully, you can run the bridge daemon to connect to your autopilot or flight simulator.

### Connection Options

The bridge uses a standard connection URL scheme (handled by `libmav`):

| Protocol | Connection URL Example | Description |
| :--- | :--- | :--- |
| **UDP** | `udp://:14550` | Bind to UDP port 14550 (receives autopilot packets) |
| **UDP (Remote)** | `udp://192.168.1.50:14550` | Send/receive packets from a specific IP/port |
| **TCP** | `tcp://127.0.0.1:5760` | Connect to a TCP server (e.g., SITL TCP port) |
| **Serial** | `serial:///dev/ttyUSB0:57600` | Connect via Serial port at 57600 baud |

### Start the Daemon

Start the bridge binding to a local UDP port and exposing the gRPC service on port `50051`:

```bash
./bridge/mavlink2grpc -c udp://:14550 -g 0.0.0.0:50051
```

---

## Running the Inspector Demo

MAVLink2gRPC includes a web-based QGroundControl-style MAVLink inspector that connects to the bridge.

### 1. Install Dependencies
```bash
cd examples/inspector
npm install
```

### 2. Run the Inspector Server
```bash
node server.js -g localhost:50051 -p 8000
```

### 3. Open Web UI
Open your browser and navigate to `http://localhost:8000`. You can monitor incoming message frequencies, browse raw fields, and plot live numeric telemetry on charts.
