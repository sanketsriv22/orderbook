cmake_minimum_required(VERSION 3.16)

# Project name and language
project(Orderbook LANGUAGES CXX C)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all sources (recursively)
file(GLOB_RECURSE SOURCES
    CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/utils/*.cpp
    # ${CMAKE_SOURCE_DIR}/external/
)

message(STATUS "Found sources: ${SOURCES}")

# Add executable
add_executable(orderbook ${SOURCES})

# Include directories
target_include_directories(orderbook PRIVATE
    ${CMAKE_SOURCE_DIR}
    # ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/utils
)

# find_package(glfw3 REQUIRED)
# target_link_libraries(orderbook PRIVATE glfw)

