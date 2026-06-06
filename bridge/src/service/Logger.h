/**
 * Copyright (C) 2025 Hüseyin Karakaya
 * This file is part of the mavlink2grpc project licensed under the MIT License.
 */

/**
 * @file Logger.h
 * @brief Asynchronous thread-safe logger for MAVLink bridge.
 *
 * Based on Revak HTTP server logger implementation.
 */

#pragma once

#include <string>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace mavlink2grpc {

/**
 * @brief Thread-safe asynchronous logger.
 *
 * Singleton logger that processes log messages in a background thread
 * to avoid blocking the main application.
 *
 * Features:
 * - Asynchronous logging (non-blocking)
 * - Thread-safe queue
 * - Timestamp with millisecond precision
 * - Color-coded log levels
 * - Graceful shutdown
 *
 * Usage:
 * @code
 * Logger::Info("MAVLink connection established");
 * Logger::Warn("Sequence gap detected");
 * Logger::Error("Transport read failed");
 * @endcode
 */
class Logger {
public:
  /**
   * @brief Get the singleton instance.
   */
  static Logger& Instance();

  // Disable copy and assignment
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  /**
   * @brief Log levels.
   */
  enum class Level {
    INFO,     ///< Informational messages
    WARNING,  ///< Warning messages
    ERROR     ///< Error messages
  };

  /**
   * @brief Log a message with the specified level.
   *
   * @param level Log level
   * @param message Message to log
   */
  void Log(Level level, const std::string& message);

  /**
   * @brief Convenience methods for different log levels.
   */
  static void Info(const std::string& message) {
    Instance().Log(Level::INFO, message);
  }

  static void Warn(const std::string& message) {
    Instance().Log(Level::WARNING, message);
  }

  static void Error(const std::string& message) {
    Instance().Log(Level::ERROR, message);
  }

private:
  /**
   * @brief Private constructor for singleton pattern.
   */
  Logger();

  /**
   * @brief Destructor - ensures logging thread is stopped.
   */
  ~Logger();

  std::mutex queue_mutex_;                  ///< Protects log queue
  std::thread log_thread_;                  ///< Background logging thread
  std::queue<std::string> log_queue_;       ///< Log message queue
  std::atomic<bool> stop_logging_{false};   ///< Stop signal
  std::condition_variable log_condition_;   ///< Notification for new logs
};

} // namespace mavlink2grpc

