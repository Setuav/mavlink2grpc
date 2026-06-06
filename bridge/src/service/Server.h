/**
 * Copyright (C) 2025 Hüseyin Karakaya
 * This file is part of the mavlink2grpc project licensed under the MIT License.
 */

#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

namespace mavlink2grpc {

class MavlinkBridgeServiceImpl;

class Server {
public:
  Server(std::shared_ptr<MavlinkBridgeServiceImpl> service, 
         const std::string& address = "0.0.0.0:50051");
  ~Server();

  // Non-copyable, non-movable
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;
  Server(Server&&) = delete;
  Server& operator=(Server&&) = delete;

  // Start the gRPC server in a background thread
  void start();

  // Stop the server gracefully
  void stop();

  // Wait for server to finish (blocks until shutdown)
  void wait();

  // Check if server is running
  bool is_running() const;

  // Get server address
  std::string address() const { return server_address_; }

private:
  std::shared_ptr<MavlinkBridgeServiceImpl> service_;
  std::unique_ptr<grpc::Server> server_;
  std::string server_address_;
  bool running_;
};

} // namespace mavlink2grpc