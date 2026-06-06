/**
 * Copyright (C) 2025 Hüseyin Karakaya
 * This file is part of the mavlink2grpc project licensed under the MIT License.
 */

#include "Router.h"
#include "Logger.h"
#include <sstream>
#include <algorithm>

namespace mavlink2grpc {

bool StreamSubscription::matches(const mavlink::MavlinkMessage& msg) const {
  // Check system ID filter (0 = all systems)
  if (filter.system_id() != 0 && msg.system_id() != filter.system_id()) {
    return false;
  }

  // Check component ID filter (0 = all components)
  if (filter.component_id() != 0 && msg.component_id() != filter.component_id()) {
    return false;
  }

  // Check message ID filter (empty = all messages)
  if (filter.message_ids_size() > 0) {
    bool found = false;
    for (int i = 0; i < filter.message_ids_size(); ++i) {
      if (msg.message_id() == filter.message_ids(i)) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }

  return true;
}

uint64_t Router::subscribe(
    const mavlink::StreamFilter& filter,
    StreamSubscription::WriteCallback write_func) {
  
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  uint64_t sub_id = next_subscription_id_++;

  subscriptions_.push_back({
    .id = sub_id,
    .write_func = std::move(write_func),
    .filter = filter,
    .active = true
  });

  {
    std::ostringstream oss;
    oss << "Stream subscribed (ID: " << sub_id
        << ", sys: " << filter.system_id()
        << ", comp: " << filter.component_id()
        << ", msgs: " << filter.message_ids_size() << ")";
    Logger::Info(oss.str());
  }

  return sub_id;
}

bool Router::unsubscribe(uint64_t subscription_id) {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  auto it = std::find_if(
    subscriptions_.begin(),
    subscriptions_.end(),
    [subscription_id](const auto& sub) { return sub.id == subscription_id; }
  );

  if (it != subscriptions_.end()) {
    std::ostringstream oss;
    oss << "Stream unsubscribed (ID: " << subscription_id << ")";
    Logger::Info(oss.str());
    subscriptions_.erase(it);
    return true;
  }

  return false;
}

size_t Router::route_message(const mavlink::MavlinkMessage& msg) {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  size_t delivered = 0;

  for (auto& sub : subscriptions_) {
    if (!sub.active) {
      continue;
    }

    // Check if message matches filter
    if (!sub.matches(msg)) {
      continue;
    }

    // Try to write to stream
    bool success = sub.write_func(msg);
    
    if (success) {
      delivered++;
    } else {
      // Stream closed or error - mark inactive
      sub.active = false;
      std::ostringstream oss;
      oss << "Stream write failed, marking inactive (ID: " << sub.id << ")";
      Logger::Warn(oss.str());
    }
  }

  return delivered;
}

size_t Router::subscription_count() const {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);
  return std::count_if(
    subscriptions_.begin(),
    subscriptions_.end(),
    [](const auto& sub) { return sub.active; }
  );
}

size_t Router::cleanup_inactive() {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  size_t before = subscriptions_.size();

  subscriptions_.erase(
    std::remove_if(
      subscriptions_.begin(),
      subscriptions_.end(),
      [](const auto& sub) { return !sub.active; }
    ),
    subscriptions_.end()
  );

  size_t removed = before - subscriptions_.size();
  
  if (removed > 0) {
    std::ostringstream oss;
    oss << "Cleaned up " << removed << " inactive subscriptions";
    Logger::Info(oss.str());
  }

  return removed;
}

} // namespace mavlink2grpc