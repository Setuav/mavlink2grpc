/**
 * Copyright (C) 2025 Hüseyin Karakaya
 * This file is part of the mavlink2grpc project licensed under the MIT License.
 */

#include "Bridge.h"
#include "mavlink/MessageConverter.h"
#include <mav/TCPClient.h>
#include <mav/TCPServer.h>
#include <mav/UDPClient.h>
#include <mav/UDPServer.h>
#include <mav/Serial.h>
#include "service/Logger.h"
#include <sstream>
#include <regex>
#include <stdexcept>
#include <vector>
#include <unistd.h>

namespace mavlink2grpc {

std::unique_ptr<mav::NetworkInterface> Bridge::parse_connection_url(const std::string& url) {
  // URL format: protocol://[host]:port or protocol://device:baudrate
  // Examples:
  //   udp://:14550 - UDP server on port 14550
  //   udp://192.168.1.100:14550 - UDP client to 192.168.1.100:14550
  //   tcp://:4560 - TCP server on port 4560
  //   tcp://192.168.1.100:4560 - TCP client to 192.168.1.100:4560
  //   serial:///dev/ttyUSB0:57600 - Serial on /dev/ttyUSB0 at 57600 baud

  std::regex udp_server_regex(R"(udp://:(\d+))");
  std::regex udp_client_regex(R"(udp://([^:]+):(\d+))");
  std::regex tcp_server_regex(R"(tcp://:(\d+))");
  std::regex tcp_client_regex(R"(tcp://([^:]+):(\d+))");
  std::regex serial_regex(R"(serial://([^:]+):(\d+))");
  
  std::smatch match;

  // UDP server (e.g., udp://:14550)
  if (std::regex_match(url, match, udp_server_regex)) {
    uint16_t port = std::stoi(match[1]);
    std::ostringstream oss;
    oss << "Connecting to MAVLink via UDP server on port " << port;
    Logger::Info(oss.str());
    return std::make_unique<mav::UDPServer>(port);
  }

  // UDP client (e.g., udp://192.168.1.100:14550)
  if (std::regex_match(url, match, udp_client_regex)) {
    std::string host = match[1];
    uint16_t port = std::stoi(match[2]);
    std::ostringstream oss;
    oss << "Connecting to MAVLink via UDP client to " << host << ":" << port;
    Logger::Info(oss.str());
    return std::make_unique<mav::UDPClient>(host, port);
  }

  // TCP server (e.g., tcp://:4560)
  if (std::regex_match(url, match, tcp_server_regex)) {
    uint16_t port = std::stoi(match[1]);
    std::ostringstream oss;
    oss << "Connecting to MAVLink via TCP server on port " << port;
    Logger::Info(oss.str());
    return std::make_unique<mav::TCPServer>(port);
  }

  // TCP client (e.g., tcp://192.168.1.100:4560)
  if (std::regex_match(url, match, tcp_client_regex)) {
    std::string host = match[1];
    uint16_t port = std::stoi(match[2]);
    std::ostringstream oss;
    oss << "Connecting to MAVLink via TCP client to " << host << ":" << port;
    Logger::Info(oss.str());
    return std::make_unique<mav::TCPClient>(host, port);
  }

  // Serial (e.g., serial:///dev/ttyUSB0:57600)
  if (std::regex_match(url, match, serial_regex)) {
    std::string device = match[1];
    uint32_t baudrate = std::stoi(match[2]);
    std::ostringstream oss;
    oss << "Connecting to MAVLink via serial " << device << " @ " << baudrate << " baud";
    Logger::Info(oss.str());
    return std::make_unique<mav::Serial>(device, baudrate);
  }

  std::ostringstream oss;
  oss << "Invalid connection URL: " << url;
  throw std::runtime_error(oss.str());
}

Bridge::Bridge(const std::string& connection_url,
               const std::string& grpc_address,
               uint8_t system_id,
               uint8_t component_id)
    : system_id_(system_id), component_id_(component_id), running_(false) {
  
  // Create router for message routing
  router_ = std::make_shared<Router>();

  // Find XML path
  std::string dialect = MAVLINK_DIALECT;
  std::string xml_filename = dialect + ".xml";
  std::string xml_path = "";
  std::vector<std::string> paths = {
    "../../third_party/mavlink/message_definitions/v1.0/" + xml_filename,
    "../third_party/mavlink/message_definitions/v1.0/" + xml_filename,
    "third_party/mavlink/message_definitions/v1.0/" + xml_filename,
    "../../mavlink/message_definitions/v1.0/" + xml_filename,
    "../mavlink/message_definitions/v1.0/" + xml_filename,
    "mavlink/message_definitions/v1.0/" + xml_filename,
    "../third_party/libmav/tests/common.xml"
  };
  for (const auto& p : paths) {
    if (access(p.c_str(), F_OK) != -1) {
      xml_path = p;
      break;
    }
  }
  if (xml_path.empty()) {
    throw std::runtime_error("Could not find " + xml_filename + " definition file.");
  }

  message_set_ = std::make_shared<mav::MessageSet>(xml_path);
  physical_interface_ = parse_connection_url(connection_url);

  auto on_conn = [this](const std::shared_ptr<mav::Connection>& connection) {
    Logger::Info("MAVLink connection established!");
    active_connection_ = connection;

    // Register callback for MAVLink messages
    connection->addMessageCallback([this](const mav::Message& msg) {
      on_mavlink_message(msg);
    });
  };

  auto on_conn_lost = [this](const std::shared_ptr<mav::Connection>& connection) {
    Logger::Warn("MAVLink connection lost!");
    if (active_connection_ == connection) {
      active_connection_.reset();
    }
  };

  const mav::Identifier own_id{system_id, component_id};
  runtime_ = std::make_unique<mav::NetworkRuntime>(own_id, *message_set_, *physical_interface_, on_conn, on_conn_lost);

  // Create gRPC service with router and send callback
  service_ = std::make_shared<MavlinkBridgeServiceImpl>(
    *router_,
    [this](const mavlink2grpc::MavlinkMessage& proto_msg) -> bool {
      if (!active_connection_ || !active_connection_->alive()) {
        Logger::Warn("Cannot send message: No active MAVLink connection");
        return false;
      }

      // Convert proto to MAVLink message
      auto mav_msg = MessageConverter::from_proto(proto_msg, *message_set_);
      if (!mav_msg) {
        Logger::Warn("Failed to convert proto message to MAVLink");
        return false;
      }
      
      try {
        active_connection_->send(*mav_msg);
        return true;
      } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Failed to send message: " << e.what();
        Logger::Error(oss.str());
        return false;
      }
    }
  );

  // Create gRPC server
  server_ = std::make_unique<Server>(service_, grpc_address);

  Logger::Info("Bridge initialized with libmav backend");
}

Bridge::~Bridge() {
  if (running_) {
    stop();
  }
}

void Bridge::start() {
  if (running_) {
    Logger::Warn("Bridge already running");
    return;
  }

  Logger::Info("Starting bridge...");

  // Start gRPC server
  server_->start();

  running_ = true;
  Logger::Info("Bridge started successfully");
}

void Bridge::stop() {
  if (!running_) {
    Logger::Warn("Bridge not running");
    return;
  }

  Logger::Info("Stopping bridge...");

  // Stop gRPC server
  server_->stop();

  // Stop libmav runtime
  if (runtime_) {
    runtime_->stop();
  }

  running_ = false;
  Logger::Info("Bridge stopped");
}

void Bridge::wait() {
  if (!running_) {
    Logger::Warn("Bridge not running");
    return;
  }

  // Wait for gRPC server to finish
  server_->wait();
}

bool Bridge::is_running() const {
  return running_;
}

void Bridge::on_mavlink_message(const mav::Message& msg) {
  // Convert MAVLink message to proto
  auto proto_msg = MessageConverter::to_proto(msg);
  if (!proto_msg) {
    // Silently ignore unsupported messages to avoid log spam
    return;
  }
  
  // Log received message
  {
    std::ostringstream oss;
    oss << "MAVLink message received: msgid=" << msg.id()
        << " (" << msg.name() << ")"
        << " from sys=" << static_cast<int>(msg.header().systemId())
        << " comp=" << static_cast<int>(msg.header().componentId());
    Logger::Info(oss.str());
  }
  
  // Route the proto message to all subscribed gRPC clients
  size_t delivered = router_->route_message(*proto_msg);
  
  if (delivered > 0) {
    std::ostringstream oss;
    oss << "  → Routed to " << delivered << " gRPC client(s)";
    Logger::Info(oss.str());
  }
}

} // namespace mavlink2grpc