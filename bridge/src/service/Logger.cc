/**
 * Copyright (C) 2025 Hüseyin Karakaya
 * This file is part of the mavlink2grpc project licensed under the MIT License.
 */

#include "Logger.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <iostream>

namespace mavlink2grpc {

Logger& Logger::Instance() {
  static Logger instance;
  return instance;
}

Logger::Logger() {
  // Start background logging thread
  log_thread_ = std::thread([this]() {
    while (true) {
      std::string message;
      
      {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        // Wait for log message or stop signal
        log_condition_.wait(lock, [this] {
          return stop_logging_.load() || !log_queue_.empty();
        });

        // If stopping and no messages left, exit
        if (stop_logging_.load() && log_queue_.empty()) {
          break;
        }

        // Get message from queue
        message = std::move(log_queue_.front());
        log_queue_.pop();
      }

      // Output to console (outside lock)
      std::cout << message << std::endl;
    }
  });
}

Logger::~Logger() {
  // Signal thread to stop
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    stop_logging_.store(true);
  }
  log_condition_.notify_all();

  // Wait for thread to finish
  if (log_thread_.joinable()) {
    log_thread_.join();
  }
}

void Logger::Log(Level level, const std::string& message) {
  // Format: [Timestamp] [Level] Message
  auto now = std::chrono::system_clock::now();
  auto now_time_t = std::chrono::system_clock::to_time_t(now);
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    now.time_since_epoch()) % 1000;
  
  std::tm tm_buf;
  localtime_r(&now_time_t, &tm_buf);
  
  std::ostringstream timestamp_oss;
  timestamp_oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
                << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
  std::string timestamp = timestamp_oss.str();

  // Color codes for terminal output
  const char* color_reset = "\033[0m";
  const char* color_code = "";
  std::string level_str;

  switch (level) {
    case Level::INFO:
      color_code = "\033[32m";  // Green
      level_str = "INFO";
      break;
    case Level::WARNING:
      color_code = "\033[33m";  // Yellow
      level_str = "WARN";
      break;
    case Level::ERROR:
      color_code = "\033[31m";  // Red
      level_str = "ERR ";
      break;
  }

  std::ostringstream log_oss;
  log_oss << color_code << "[" << timestamp << "] [" 
          << level_str << "]" << color_reset << " " << message;
  std::string log_entry = log_oss.str();

  // Add to queue
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    log_queue_.push(std::move(log_entry));
  }
  log_condition_.notify_one();
}

} // namespace mavlink2grpc