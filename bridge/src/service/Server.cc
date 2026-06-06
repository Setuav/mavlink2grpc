/**
 * Copyright (C) 2025 Hüseyin Karakaya
 * This file is part of the mavlink2grpc project licensed under the MIT License.
 */

#include "Server.h"
#include "Service.h"
#include "Logger.h"
#include <sstream>
#include <thread>

namespace mavlink2grpc {

Server::Server(std::shared_ptr<MavlinkBridgeServiceImpl> service, 
               const std::string& address)
    : service_(service), server_address_(address), running_(false) {
  if (!service_) {
    throw std::invalid_argument("Service cannot be null");
  }
}

Server::~Server() {
  if (running_) {
    stop();
  }
}

void Server::start() {
  if (running_) {
    Logger::Warn("Server already running");
    return;
  }

  grpc::ServerBuilder builder;
  
  // Listen on the specified address without authentication
  builder.AddListeningPort(server_address_, grpc::InsecureServerCredentials());
  
  // Register the service
  builder.RegisterService(service_.get());
  
  // Build and start the server
  server_ = builder.BuildAndStart();
  
  if (!server_) {
    std::ostringstream oss;
    oss << "Failed to start server on " << server_address_;
    Logger::Error(oss.str());
    throw std::runtime_error("Failed to start gRPC server");
  }
  
  running_ = true;
  {
    std::ostringstream oss;
    oss << "gRPC server listening on " << server_address_;
    Logger::Info(oss.str());
  }
}

void Server::stop() {
  if (!running_) {
    Logger::Warn("Server not running");
    return;
  }

  Logger::Info("Shutting down gRPC server...");

  // Notify service to shutdown all active streams
  if (service_) {
    service_->shutdown();
  }

  // Shutdown the server with a deadline
  if (server_) {
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    server_->Shutdown(deadline);
  }

  running_ = false;
  Logger::Info("gRPC server stopped");
}

void Server::wait() {
  if (!server_) {
    Logger::Warn("Server not initialized");
    return;
  }
  
  if (!running_) {
    Logger::Warn("Server not running");
    return;
  }
  
  Logger::Info("Waiting for server to finish...");
  server_->Wait();
}

bool Server::is_running() const {
  return running_;
}

} // namespace mavlink2grpc