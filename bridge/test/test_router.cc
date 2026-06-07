#include "service/Router.h"
#include <iostream>
#include <cstdlib>

#define TEST_ASSERT(cond) \
  do { \
    if (!(cond)) { \
      std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
      std::abort(); \
    } \
  } while (0)

using namespace mavlink2grpc;

void test_router_basic() {
  std::cout << "Running test_router_basic..." << std::endl;
  Router router;

  TEST_ASSERT(router.subscription_count() == 0);

  // Subscribe client 1 with no filters (system_id=0, component_id=0)
  mavlink2grpc::StreamFilter filter1;
  filter1.set_system_id(0);
  filter1.set_component_id(0);

  size_t client1_received = 0;
  uint64_t sub1 = router.subscribe(filter1, [&](const mavlink2grpc::MavlinkMessage&) {
    client1_received++;
    return true;
  });

  TEST_ASSERT(sub1 == 1);
  TEST_ASSERT(router.subscription_count() == 1);

  // Send a message
  mavlink2grpc::MavlinkMessage msg1;
  msg1.set_system_id(1);
  msg1.set_component_id(1);
  msg1.set_message_id(0); // HEARTBEAT

  size_t routed = router.route_message(msg1);
  TEST_ASSERT(routed == 1);
  TEST_ASSERT(client1_received == 1);

  // Unsubscribe client 1
  bool unsub_ok = router.unsubscribe(sub1);
  TEST_ASSERT(unsub_ok);
  TEST_ASSERT(router.subscription_count() == 0);

  routed = router.route_message(msg1);
  TEST_ASSERT(routed == 0);
  TEST_ASSERT(client1_received == 1); // should remain 1

  std::cout << "test_router_basic passed!" << std::endl;
}

void test_router_filters() {
  std::cout << "Running test_router_filters..." << std::endl;
  Router router;

  // Filter for system 1, component 1, message 30 (ATTITUDE) only
  mavlink2grpc::StreamFilter filter;
  filter.set_system_id(1);
  filter.set_component_id(1);
  filter.add_message_ids(30);

  size_t received = 0;
  router.subscribe(filter, [&](const mavlink2grpc::MavlinkMessage&) {
    received++;
    return true;
  });

  // Message 1: Matches all
  mavlink2grpc::MavlinkMessage msg1;
  msg1.set_system_id(1);
  msg1.set_component_id(1);
  msg1.set_message_id(30);
  router.route_message(msg1);
  TEST_ASSERT(received == 1);

  // Message 2: Different system ID (2)
  mavlink2grpc::MavlinkMessage msg2;
  msg2.set_system_id(2);
  msg2.set_component_id(1);
  msg2.set_message_id(30);
  router.route_message(msg2);
  TEST_ASSERT(received == 1); // shouldn't increase

  // Message 3: Different component ID (2)
  mavlink2grpc::MavlinkMessage msg3;
  msg3.set_system_id(1);
  msg3.set_component_id(2);
  msg3.set_message_id(30);
  router.route_message(msg3);
  TEST_ASSERT(received == 1); // shouldn't increase

  // Message 4: Different message ID (0 - HEARTBEAT)
  mavlink2grpc::MavlinkMessage msg4;
  msg4.set_system_id(1);
  msg4.set_component_id(1);
  msg4.set_message_id(0);
  router.route_message(msg4);
  TEST_ASSERT(received == 1); // shouldn't increase

  std::cout << "test_router_filters passed!" << std::endl;
}

void test_router_cleanup() {
  std::cout << "Running test_router_cleanup..." << std::endl;
  Router router;

  mavlink2grpc::StreamFilter filter;
  
  // Client whose write fails (simulates disconnected client)
  router.subscribe(filter, [](const mavlink2grpc::MavlinkMessage&) {
    return false; // write failed
  });

  TEST_ASSERT(router.subscription_count() == 1);

  // Route a message - this will trigger write failure and mark inactive
  mavlink2grpc::MavlinkMessage msg;
  size_t routed = router.route_message(msg);
  
  TEST_ASSERT(routed == 0);
  TEST_ASSERT(router.subscription_count() == 0); // Active subscription count should drop to 0

  // Cleanup should remove it from internal list
  size_t cleaned = router.cleanup_inactive();
  TEST_ASSERT(cleaned == 1);

  std::cout << "test_router_cleanup passed!" << std::endl;
}

void run_router_tests() {
  test_router_basic();
  test_router_filters();
  test_router_cleanup();
}
