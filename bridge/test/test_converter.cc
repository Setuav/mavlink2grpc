#include "mavlink/MessageConverter.h"
#include <mav/MessageSet.h>
#include <iostream>
#include <cstdlib>
#include <unistd.h>

#define TEST_ASSERT(cond) \
  do { \
    if (!(cond)) { \
      std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
      std::abort(); \
    } \
  } while (0)

using namespace mavlink2grpc;

void test_message_converter_heartbeat() {
  std::cout << "Running test_message_converter_heartbeat..." << std::endl;

  // Locate the message definition file
  std::string xml_path = "";
  std::vector<std::string> paths = {
    "third_party/mavlink/message_definitions/v1.0/common.xml",
    "../third_party/mavlink/message_definitions/v1.0/common.xml",
    "mavlink/message_definitions/v1.0/common.xml",
    "../mavlink/message_definitions/v1.0/common.xml"
  };

  for (const auto& p : paths) {
    if (access(p.c_str(), F_OK) != -1) {
      xml_path = p;
      break;
    }
  }

  if (xml_path.empty()) {
    std::cerr << "Warning: Could not find common.xml definition. Skipping converter tests." << std::endl;
    return;
  }

  // Load MessageSet
  mav::MessageSet message_set(xml_path);

  // 1. Create a mav::Message for HEARTBEAT
  mav::Message mav_msg = message_set.create("HEARTBEAT");
  TEST_ASSERT(mav_msg.name() == "HEARTBEAT");
  TEST_ASSERT(mav_msg.id() == 0);

  // Set header partner ID (represents system ID and component ID)
  // libmav sets source system/component via setFromConnectionPartner
  // Let's set some fields on the message
  mav_msg.set("type", static_cast<uint8_t>(2)); // MAV_TYPE_QUADROTOR
  mav_msg.set("autopilot", static_cast<uint8_t>(3)); // MAV_AUTOPILOT_ARDUPILOTMEGA
  mav_msg.set("base_mode", static_cast<uint8_t>(81));
  mav_msg.set("custom_mode", static_cast<uint32_t>(12345));
  mav_msg.set("system_status", static_cast<uint8_t>(3)); // MAV_STATE_STANDBY
  mav_msg.set("mavlink_version", static_cast<uint8_t>(3));

  // 2. Convert to Protobuf
  // We can pass system_id = 42 and component_id = 191 by simulating a connection partner if needed,
  // but let's check what default to_proto does or if it gets them from the message source.
  auto proto_opt = MessageConverter::to_proto(mav_msg, 123456789ULL);
  TEST_ASSERT(proto_opt.has_value());

  auto proto_msg = proto_opt.value();
  TEST_ASSERT(proto_msg.message_id() == 0);
  TEST_ASSERT(proto_msg.timestamp_usec() == 123456789ULL);
  TEST_ASSERT(proto_msg.has_heartbeat());

  const auto& heartbeat = proto_msg.heartbeat();
  TEST_ASSERT(heartbeat.type() == 2);
  TEST_ASSERT(heartbeat.autopilot() == 3);
  TEST_ASSERT(heartbeat.base_mode() == 81);
  TEST_ASSERT(heartbeat.custom_mode() == 12345);
  TEST_ASSERT(heartbeat.system_status() == 3);

  // 3. Convert back from Protobuf to mav::Message
  auto back_opt = MessageConverter::from_proto(proto_msg, message_set);
  TEST_ASSERT(back_opt.has_value());

  auto back_msg = back_opt.value();
  TEST_ASSERT(back_msg.name() == "HEARTBEAT");
  TEST_ASSERT(back_msg.id() == 0);

  TEST_ASSERT(back_msg.get<uint8_t>("type") == 2);
  TEST_ASSERT(back_msg.get<uint8_t>("autopilot") == 3);
  TEST_ASSERT(back_msg.get<uint8_t>("base_mode") == 81);
  TEST_ASSERT(back_msg.get<uint32_t>("custom_mode") == 12345);
  TEST_ASSERT(back_msg.get<uint8_t>("system_status") == 3);

  std::cout << "test_message_converter_heartbeat passed!" << std::endl;
}

void run_converter_tests() {
  test_message_converter_heartbeat();
}
