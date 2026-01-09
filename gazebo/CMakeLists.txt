cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(flix_gazebo)

# Gazebo plugin
find_package(gazebo REQUIRED)
find_package(SDL2 REQUIRED)
include_directories(${GAZEBO_INCLUDE_DIRS})
link_directories(${GAZEBO_LIBRARY_DIRS})
list(APPEND CMAKE_CXX_FLAGS "${GAZEBO_CXX_FLAGS}")

set(FLIX_SOURCE_DIR ../flix)
include_directories(${FLIX_SOURCE_DIR})
set(FLIX_SOURCE_DIR ../flix)
include_directories(${FLIX_SOURCE_DIR})
set(FLIX_SOURCES
  ${FLIX_SOURCE_DIR}/cli.cpp
  ${FLIX_SOURCE_DIR}/control.cpp
  ${FLIX_SOURCE_DIR}/estimate.cpp
  ${FLIX_SOURCE_DIR}/safety.cpp
  ${FLIX_SOURCE_DIR}/log.cpp
  ${FLIX_SOURCE_DIR}/mavlink.cpp
  ${FLIX_SOURCE_DIR}/motors.cpp
  ${FLIX_SOURCE_DIR}/parameters.cpp
  ${FLIX_SOURCE_DIR}/rc.cpp
  ${FLIX_SOURCE_DIR}/time.cpp
)

set(CMAKE_BUILD_TYPE RelWithDebInfo)
add_library(flix SHARED simulator.cpp ${FLIX_SOURCES})
target_link_libraries(flix ${GAZEBO_LIBRARIES} ${SDL2_LIBRARIES})
target_include_directories(flix PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(flix PRIVATE -Wno-address-of-packed-member) # disable unneeded mavlink warnings

# Include dir for MAVLink-Arduino library
target_include_directories(flix PUBLIC $ENV{HOME}/Arduino/libraries/MAVLink)
target_include_directories(flix PUBLIC $ENV{HOME}/Documents/Arduino/libraries/MAVLink)
