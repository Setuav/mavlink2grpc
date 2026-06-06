/**
 * Copyright (C) 2025 Hüseyin Karakaya
 * This file is part of the mavlink2grpc project licensed under the MIT License.
 */

/**
 * @file Router.h
 * @brief Routes MAVLink messages to subscribed gRPC streams with filtering.
 */

#pragma once

#include <mavlink_bridge.pb.h>

#include <vector>
#include <mutex>
#include <memory>
#include <functional>

namespace mavlink2grpc {

/**
 * @brief Subscription to a message stream with filtering.
 *
 * Represents a single gRPC client stream subscription with
 * optional filtering by system ID, component ID, and message IDs.
 */
struct StreamSubscription {
  using WriteCallback = std::function<bool(const mavlink::MavlinkMessage&)>;

  uint64_t id;                           ///< Unique subscription ID
  WriteCallback write_func;              ///< Function to write message to stream
  mavlink::StreamFilter filter;          ///< Filter criteria
  bool active;                           ///< Is subscription still active

  /**
   * @brief Check if message matches filter criteria.
   *
   * @param msg Message to check
   * @return true if message passes filter, false otherwise
   */
  bool matches(const mavlink::MavlinkMessage& msg) const;
};

/**
 * @brief Routes MAVLink messages to subscribed streams.
 *
 * Thread-safe router that maintains a list of active stream subscriptions
 * and delivers messages to matching subscribers.
 *
 * Features:
 * - Per-stream filtering (system ID, component ID, message IDs)
 * - Automatic cleanup of dead/cancelled streams
 * - Thread-safe subscription management
 * - Efficient message routing
 *
 * Usage:
 * @code
 * Router router;
 * 
 * // Subscribe a stream
 * auto sub_id = router.subscribe(filter, [writer](const auto& msg) {
 *   return writer->Write(msg);
 * });
 * 
 * // Route incoming message
 * router.route_message(proto_msg);
 * 
 * // Unsubscribe when done
 * router.unsubscribe(sub_id);
 * @endcode
 */
class Router {
public:
  Router() = default;
  ~Router() = default;

  // Disable copy and assignment
  Router(const Router&) = delete;
  Router& operator=(const Router&) = delete;

  /**
   * @brief Subscribe to message stream.
   *
   * @param filter Filter criteria for this subscription
   * @param write_func Callback to write message to stream (returns false if stream closed)
   * @return Subscription ID (use for unsubscribe)
   */
  uint64_t subscribe(
    const mavlink::StreamFilter& filter,
    StreamSubscription::WriteCallback write_func);

  /**
   * @brief Unsubscribe from message stream.
   *
   * @param subscription_id ID returned by subscribe()
   * @return true if subscription was found and removed
   */
  bool unsubscribe(uint64_t subscription_id);

  /**
   * @brief Route message to all matching subscriptions.
   *
   * Delivers message to all active subscribers whose filters match.
   * Automatically removes subscriptions with closed streams.
   *
   * @param msg Message to route
   * @return Number of subscribers that received the message
   */
  size_t route_message(const mavlink::MavlinkMessage& msg);

  /**
   * @brief Get number of active subscriptions.
   */
  size_t subscription_count() const;

  /**
   * @brief Remove all inactive subscriptions.
   *
   * @return Number of subscriptions removed
   */
  size_t cleanup_inactive();

private:
  mutable std::mutex subscriptions_mutex_;           ///< Protects subscriptions
  std::vector<StreamSubscription> subscriptions_;    ///< Active subscriptions
  uint64_t next_subscription_id_{1};                 ///< Next ID to assign
};

} // namespace mavlink2grpc