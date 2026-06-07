/**
 * Copyright (C) 2025 Hüseyin Karakaya
 * This file is part of the mavlink2grpc project licensed under the MIT License.
 */

#include "Service.h"
#include "Logger.h"
#include <sstream>
#include <chrono>

namespace mavlink2grpc {

MavlinkBridgeServiceImpl::MavlinkBridgeServiceImpl(
    Router& router,
    SendMessageCallback send_callback)
  : router_(router),
    send_callback_(std::move(send_callback)) {
}

grpc::Status MavlinkBridgeServiceImpl::StreamMessages(
    grpc::ServerContext* context,
    const mavlink2grpc::StreamFilter* request,
    grpc::ServerWriter<mavlink2grpc::MavlinkMessage>* writer) {
  
  {
    std::ostringstream oss;
    oss << "Client connected - StreamMessages (sys:" << request->system_id()
        << ", comp:" << request->component_id()
        << ", msgs:" << request->message_ids_size() << ")";
    Logger::Info(oss.str());
  }

  // Subscribe to router with filter
  uint64_t sub_id = router_.subscribe(
    *request,
    [writer](const mavlink2grpc::MavlinkMessage& msg) -> bool {
      // Write to gRPC stream
      return writer->Write(msg);
    }
  );

  // Wait for client cancellation or server shutdown
  // Use wait_for with timeout to handle both:
  // - Server shutdown: notify_all() wakes immediately
  // - Client disconnect: timeout ensures we check IsCancelled() regularly
  std::unique_lock<std::mutex> lock(shutdown_mtx_);
  while (!shutting_down_.load() && !context->IsCancelled()) {
    shutdown_cv_.wait_for(lock, std::chrono::milliseconds(100));
  }

  // Cleanup subscription
  router_.unsubscribe(sub_id);

  {
    std::ostringstream oss;
    oss << "Client disconnected - StreamMessages (ID: " << sub_id << ")";
    Logger::Info(oss.str());
  }

  return grpc::Status::OK;
}

grpc::Status MavlinkBridgeServiceImpl::SendMessage(
    grpc::ServerContext* /* context */,
    const mavlink2grpc::MavlinkMessage* request,
    mavlink2grpc::SendResponse* response) {
  
  // Validate request has payload
  if (request->payload_case() == mavlink2grpc::MavlinkMessage::PAYLOAD_NOT_SET) {
    response->set_success(false);
    response->set_error("No payload in message");
    Logger::Warn("SendMessage RPC failed: No payload");
    return grpc::Status(
      grpc::StatusCode::INVALID_ARGUMENT,
      "Message has no payload"
    );
  }

  // Send via callback
  bool success = send_callback_(*request);

  if (success) {
    response->set_success(true);
    std::ostringstream oss;
    oss << "Sent message (ID: " << request->message_id()
        << ", sys: " << request->system_id()
        << ", comp: " << request->component_id() << ")";
    Logger::Info(oss.str());
    return grpc::Status::OK;
  } else {
    response->set_success(false);
    response->set_error("Failed to send via MAVLink connection");
    std::ostringstream oss;
    oss << "Failed to send message (ID: " << request->message_id() << ")";
    Logger::Error(oss.str());
    return grpc::Status(
      grpc::StatusCode::INTERNAL,
      "MAVLink send failed"
    );
  }
}

void MavlinkBridgeServiceImpl::shutdown() {
  Logger::Info("Service shutting down, notifying all active streams...");
  shutting_down_.store(true);
  shutdown_cv_.notify_all();
}

} // namespace mavlink2grpc