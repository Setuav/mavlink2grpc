# Developer Guide

This guide is for developers who want to modify the code generator, extend the C++ bridge, or write custom MAVLink message templates.

---

## Modifying Generator Templates

All Protocol Buffer files and C++ serialization codes are generated from Jinja2 templates located in the `generator/templates/` folder:

*   **`dialect.proto.j2`**: Template for generating individual dialect proto files (defines messages and enums).
*   **`bridge_service.proto.j2`**: Template for the main `MavlinkBridge` service (defines the RPC calls and the main `oneof payload` wrapper).
*   **`message_converter.h.j2` / `message_converter.cc.j2`**: Templates for the C++ class that handles conversion between `libmav` key-value pairs and compiled C++ Protobuf objects.

---

## Adding Custom Dialects

If you are using a custom MAVLink XML dialect:

1.  Copy your custom dialect XML file to `third_party/mavlink/message_definitions/v1.0/` (or specify the directory using the `--xml-dir` flag).
2.  Add your dialect name to choices inside `generator/main.py`.
3.  Configure CMake using your custom dialect:
    ```bash
    cmake .. -DMAVLINK_DIALECT=my_custom_dialect
    ```
4.  Rebuild. CMake will run the python generator and rebuild the bridge supporting your custom messages.

---

## Running Tests

Before committing any changes, ensure all tests pass.

### 1. Python Generator Tests
Performs dialect parsing, code generation, and runs `protoc` compiler validation across core dialects:
```bash
python3 generator/test/test_generator.py
```

### 2. C++ Bridge Unit Tests
Compiles and runs bridge unit tests (covering the lock-free message router and converter code):
```bash
cd build
make -j$(nproc) test_runner
ctest --output-on-failure
```

---

## C++ API Documentation (Docstrings)

All core C++ source code is written with clean, standard Doxygen-style docstrings.

### Documentation Conventions
Classes, structs, and public methods must contain Doxygen blocks:

```cpp
/**
 * @brief Auto-generated converter for MAVLink messages.
 * 
 * Converts between libmav Message objects and Protocol Buffer messages.
 */
class MessageConverter {
public:
  /**
   * @brief Converts libmav Message to Protocol Buffer.
   * 
   * @param mavlink_msg Source libmav message
   * @param timestamp_usec Optional custom timestamp in microseconds
   * @return Populated MavlinkMessage wrapper, or nullopt if translation fails
   */
  static std::optional<mavlink2grpc::MavlinkMessage> 
  to_proto(const mav::Message& mavlink_msg,
           std::optional<uint64_t> timestamp_usec = std::nullopt);
};
```

These docstrings can be parsed using tools like **Doxygen** or **Sphinx/Breathe** to build static C++ API references if required.
