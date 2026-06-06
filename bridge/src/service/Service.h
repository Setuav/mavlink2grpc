/**
 * Copyright (C) 2025 Hüseyin Karakaya
 * This file is part of the mavlink2grpc project licensed under the MIT License.
 */

/**
 * @file Service.h
 * @brief gRPC service implementation for MAVLink bridge.
 */

#pragma once

#include "Router.h"

#include <mavlink_bridge.grpc.pb.h>
#include <grpcpp/grpcpp.h>

#include <memory>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace mavlink2grpc {

/**
 * @brief Implementation of MavlinkBridge gRPC service.
 *
 * Provides two RPC methods:
 * - StreamMessages: Server-streaming RPC that delivers MAVLink messages to clients
 * - SendMessage: Unary RPC that sends MAVLink messages to connected systems
 *
 * Thread-safe and supports multiple concurrent clients.
 */
class MavlinkBridgeServiceImpl final : public mavlink::MavlinkBridge::Service {
public:
  /**
   * @brief Callback type for sending MAVLink messages.
   *
   * @param msg Proto message to convert and send
   * @return true if sent successfully, false otherwise
   */
  using SendMessageCallback = std::function<bool(const mavlink::MavlinkMessage&)>;

  /**
   * @brief Construct service with router and send callback.
   *
   * @param router Message router for stream subscriptions
   * @param send_callback Callback to send messages via MAVLink connection
   */
  explicit MavlinkBridgeServiceImpl(
    Router& router,
    SendMessageCallback send_callback);

  /**
   * @brief Stream MAVLink messages to client.
   *
   * Server-streaming RPC that subscribes client to filtered message stream.
   * Runs until client cancels or connection is lost.
   *
   * @param context gRPC context
   * @param request Stream filter criteria
   * @param writer Stream writer for sending messages to client
   * @return gRPC status
   */
  grpc::Status StreamMessages(
    grpc::ServerContext* context,
    const mavlink::StreamFilter* request,
    grpc::ServerWriter<mavlink::MavlinkMessage>* writer) override;

  /**
   * @brief Send MAVLink message to connected system.
   *
   * Unary RPC that sends a single message via MAVLink connection.
   *
   * @param context gRPC context
   * @param request Message to send
   * @param response Send result
   * @return gRPC status
   */
  grpc::Status SendMessage(
    grpc::ServerContext* context,
    const mavlink::MavlinkMessage* request,
    mavlink::SendResponse* response) override;

  /**
   * @brief Shutdown the service and notify all active streams.
   *
   * Called during server shutdown to wake up all waiting streams.
   */
  void shutdown();

private:
  Router& router_;                        ///< Message router
  SendMessageCallback send_callback_;     ///< Callback to send messages

  std::atomic<bool> shutting_down_{false};  ///< Shutdown flag
  std::condition_variable shutdown_cv_;     ///< Condition variable for shutdown
  std::mutex shutdown_mtx_;                 ///< Mutex for shutdown synchronization
};

} // namespace mavlink2grpc