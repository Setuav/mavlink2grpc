# Copyright (C) 2025 Hüseyin Karakaya
# This file is part of the mavlink2grpc project licensed under the MIT License.

# CMakeLists.txt for bridge module

cmake_minimum_required(VERSION 3.10)
project(mavlink2grpc VERSION 0.1.0 LANGUAGES CXX)

# Configure Version.h using project version
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/Version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h"
)

find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)
pkg_check_modules(GRPCPP REQUIRED grpc++>=1.16.0)

find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)
message(STATUS "Found gRPC C++ plugin: ${GRPC_CPP_PLUGIN}")

# Protobuf generation
set(PROTO_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../proto)
set(GENERATED_PROTOBUF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../generated)

file(MAKE_DIRECTORY ${GENERATED_PROTOBUF_PATH})
file(MAKE_DIRECTORY ${GENERATED_PROTOBUF_PATH}/mavlink2grpc)
file(MAKE_DIRECTORY ${GENERATED_PROTOBUF_PATH}/mavlink2grpc/mavlink)

# Collect all proto files
file(GLOB MAVLINK_PROTO_FILES "${PROTO_PATH}/mavlink2grpc/mavlink/*.proto")
file(GLOB ROOT_PROTO_FILES "${PROTO_PATH}/mavlink2grpc/*.proto")

# Combine all proto files and their outputs
set(ALL_PROTO_FILES ${MAVLINK_PROTO_FILES} ${ROOT_PROTO_FILES})
set(PROTO_SRCS "")
set(PROTO_HDRS "")

foreach(PROTO_FILE ${ALL_PROTO_FILES})
  file(RELATIVE_PATH PROTO_REL_PATH ${PROTO_PATH} ${PROTO_FILE})
  get_filename_component(PROTO_DIR ${PROTO_REL_PATH} DIRECTORY)
  get_filename_component(PROTO_FILENAME ${PROTO_FILE} NAME_WE)

  if(PROTO_DIR)
    list(APPEND PROTO_SRCS "${GENERATED_PROTOBUF_PATH}/${PROTO_DIR}/${PROTO_FILENAME}.pb.cc")
    list(APPEND PROTO_SRCS "${GENERATED_PROTOBUF_PATH}/${PROTO_DIR}/${PROTO_FILENAME}.grpc.pb.cc")
    list(APPEND PROTO_HDRS "${GENERATED_PROTOBUF_PATH}/${PROTO_DIR}/${PROTO_FILENAME}.pb.h")
    list(APPEND PROTO_HDRS "${GENERATED_PROTOBUF_PATH}/${PROTO_DIR}/${PROTO_FILENAME}.grpc.pb.h")
  else()
    list(APPEND PROTO_SRCS "${GENERATED_PROTOBUF_PATH}/${PROTO_FILENAME}.pb.cc")
    list(APPEND PROTO_SRCS "${GENERATED_PROTOBUF_PATH}/${PROTO_FILENAME}.grpc.pb.cc")
    list(APPEND PROTO_HDRS "${GENERATED_PROTOBUF_PATH}/${PROTO_FILENAME}.pb.h")
    list(APPEND PROTO_HDRS "${GENERATED_PROTOBUF_PATH}/${PROTO_FILENAME}.grpc.pb.h")
  endif()
endforeach()

# Generate all proto files in one command to handle dependencies
add_custom_command(
  OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
  COMMAND ${Protobuf_PROTOC_EXECUTABLE}
  ARGS --grpc_out=${GENERATED_PROTOBUF_PATH}
       --cpp_out=${GENERATED_PROTOBUF_PATH}
       --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
       -I ${PROTO_PATH}
       ${ALL_PROTO_FILES}
  DEPENDS ${ALL_PROTO_FILES}
  COMMENT "Generating protobuf and gRPC code for all proto files"
)

# Generated protobuf library
add_library(proto_lib OBJECT
  ${PROTO_SRCS}
)

target_include_directories(proto_lib PUBLIC
  ${GENERATED_PROTOBUF_PATH}
  ${Protobuf_INCLUDE_DIRS}
)

file(GLOB BRIDGE_SRCS
  src/*.cc
  src/mavlink/*.cc
  src/service/*.cc
)

# Bridge executable
add_executable(${PROJECT_NAME}
  ${BRIDGE_SRCS}
  $<TARGET_OBJECTS:proto_lib>
)

set(LIBMAV_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/libmav/include)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${GENERATED_PROTOBUF_PATH}
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# libmav headers as system includes to suppress warnings
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
  ${LIBMAV_INCLUDE_DIR}
)

add_dependencies(${PROJECT_NAME} proto_lib)

target_link_libraries(${PROJECT_NAME} PRIVATE
  ${Protobuf_LIBRARIES}
  ${GRPC_LIBRARIES}
  ${GRPCPP_LIBRARIES}
  Threads::Threads
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

if (NOT MAVLINK_DIALECT)
  set(MAVLINK_DIALECT "common" CACHE STRING "MAVLink dialect to generate")
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE MAVLINK_DIALECT="${MAVLINK_DIALECT}")

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
)

# Strict warnings for our code only (not third-party headers)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(${PROJECT_NAME} PRIVATE -O0 -g)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -O3)
endif()

# ============================================================================
# C++ Testing Target
# ============================================================================
enable_testing()

add_executable(test_runner
  test/test_runner.cc
  test/test_router.cc
  test/test_converter.cc
  src/service/Router.cc
  src/service/Logger.cc
  src/mavlink/MessageConverter.cc
  $<TARGET_OBJECTS:proto_lib>
)

target_include_directories(test_runner PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${GENERATED_PROTOBUF_PATH}
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_include_directories(test_runner SYSTEM PRIVATE
  ${LIBMAV_INCLUDE_DIR}
)

add_dependencies(test_runner proto_lib)

target_link_libraries(test_runner PRIVATE
  ${Protobuf_LIBRARIES}
  ${GRPC_LIBRARIES}
  ${GRPCPP_LIBRARIES}
  Threads::Threads
)

set_target_properties(test_runner PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

target_compile_definitions(test_runner PRIVATE MAVLINK_DIALECT="${MAVLINK_DIALECT}")
target_compile_options(test_runner PRIVATE -Wall -Wextra -Werror)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(test_runner PRIVATE -O0 -g)
else()
  target_compile_options(test_runner PRIVATE -O3)
endif()

add_test(NAME bridge_tests COMMAND test_runner)