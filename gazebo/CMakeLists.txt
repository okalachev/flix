cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(flix_gazebo)

# === gazebo plugin
find_package(gazebo REQUIRED)
find_package(SDL2 REQUIRED)
include_directories(${GAZEBO_INCLUDE_DIRS})
link_directories(${GAZEBO_LIBRARY_DIRS})
list(APPEND CMAKE_CXX_FLAGS "${GAZEBO_CXX_FLAGS}")

set(FLIX_SOURCE_DIR ../flix)
include_directories(${FLIX_SOURCE_DIR})
file(GLOB_RECURSE FLIX_INO_FILES ${FLIX_SOURCE_DIR}/*.ino)

set(CMAKE_BUILD_TYPE RelWithDebInfo)
add_library(flix SHARED flix.cpp)
target_link_libraries(flix ${GAZEBO_LIBRARIES} ${SDL2_LIBRARIES})
target_include_directories(flix PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
