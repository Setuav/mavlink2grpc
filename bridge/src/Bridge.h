/**
 * Copyright (C) 2025 Hüseyin Karakaya
 * This file is part of the mavlink2grpc project licensed under the MIT License.
 */

#pragma once

#include <mav/MessageSet.h>
#include <mav/Network.h>
#include "service/Router.h"
#include "service/Service.h"
#include "service/Server.h"
#include <memory>
#include <string>

namespace mav2grpc {

/**
 * @brief Connection configuration.
 */
struct ConnectionConfig {
  std::string connection_url;    ///< Connection URL (e.g., "udp://:14550", "serial:///dev/ttyUSB0:57600")
  std::string grpc_address;      ///< gRPC server address
  uint8_t system_id;             ///< MAVLink system ID
  uint8_t component_id;          ///< MAVLink component ID
};

/**
 * @brief Main bridge coordinator.
 *
 * Orchestrates all components:
 * - MAVLink Connection (receive MAVLink messages)
 * - Router (route messages to gRPC streams)
 * - Service (gRPC service implementation)
 * - Server (gRPC server lifecycle)
 *
 * Usage:
 * @code
 * Bridge bridge("udp://:14550");
 * bridge.start();
 * bridge.wait(); // blocks until shutdown
 * @endcode
 */
class Bridge {
public:
  /**
   * @brief Construct bridge from connection URL.
   *
   * Supported URL formats:
   * - udp://[host]:port - UDP server (e.g., "udp://:14550")
   * - udp://host:port - UDP client (e.g., "udp://192.168.1.100:14550")
   * - tcp://host:port - TCP client (e.g., "tcp://192.168.1.100:4560")
   * - tcp://:port - TCP server (e.g., "tcp://:4560")
   * - serial://device:baudrate - Serial connection (e.g., "serial:///dev/ttyUSB0:57600")
   *
   * @param connection_url MAVLink connection URL
   * @param grpc_address gRPC server address (default: 0.0.0.0:50051)
   * @param system_id MAVLink system ID (default: 1)
   * @param component_id MAVLink component ID (default: 1)
   */
  explicit Bridge(const std::string& connection_url,
                  const std::string& grpc_address = "0.0.0.0:50051",
                  uint8_t system_id = 1,
                  uint8_t component_id = 1);

  ~Bridge();

  // Non-copyable, non-movable
  Bridge(const Bridge&) = delete;
  Bridge& operator=(const Bridge&) = delete;
  Bridge(Bridge&&) = delete;
  Bridge& operator=(Bridge&&) = delete;

  /**
   * @brief Start the bridge (MAVLink connection + gRPC server).
   */
  void start();

  /**
   * @brief Stop the bridge gracefully.
   */
  void stop();

  /**
   * @brief Wait for bridge to finish (blocks until shutdown).
   */
  void wait();

  /**
   * @brief Check if bridge is running.
   */
  bool is_running() const;

private:
  /**
   * @brief Parse connection URL and create network interface.
   */
  static std::unique_ptr<mav::NetworkInterface> parse_connection_url(const std::string& url);

  /**
   * @brief MAVLink message callback - routes to gRPC clients.
   */
  void on_mavlink_message(const mav::Message& msg);

  std::shared_ptr<mav::MessageSet> message_set_;
  std::unique_ptr<mav::NetworkInterface> physical_interface_;
  std::unique_ptr<mav::NetworkRuntime> runtime_;
  std::shared_ptr<mav::Connection> active_connection_;

  std::shared_ptr<Router> router_;
  std::shared_ptr<MavlinkBridgeServiceImpl> service_;
  std::unique_ptr<Server> server_;
  uint8_t system_id_;
  uint8_t component_id_;
  bool running_;
};

} // namespace mav2grpc