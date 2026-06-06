cmake_minimum_required(VERSION 3.10)
project(mavlink2grpc_root LANGUAGES CXX)

# MAVLink dialect option (defaults to common)
set(MAVLINK_DIALECT "common" CACHE STRING "MAVLink dialect to generate (minimal, standard, common)")

message(STATUS "Configuring mavlink2grpc with MAVLink dialect: ${MAVLINK_DIALECT}")

# Run Python generator at configure-time
execute_process(
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/generator/main.py --dialect ${MAVLINK_DIALECT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE GEN_RESULT
)

if(NOT GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "MAVLink to Protobuf generation failed!")
endif()

# Add bridge module subdirectory
add_subdirectory(bridge)
