cmake_minimum_required(VERSION 3.15)
project(csv_to_ulog)
include(FetchContent)
set(CMAKE_CXX_STANDARD 17)

FetchContent_Declare(
  ulog_cpp
  GIT_REPOSITORY https://github.com/PX4/ulog_cpp.git
  GIT_TAG cf24ec6
)

FetchContent_Declare(
  rapidcsv
  GIT_REPOSITORY https://github.com/d99kris/rapidcsv.git
  GIT_TAG v8.82
)

FetchContent_MakeAvailable(ulog_cpp)
FetchContent_MakeAvailable(rapidcsv)

add_executable(csv_to_ulog csv_to_ulog.cpp)
target_link_libraries(csv_to_ulog PUBLIC ulog_cpp::ulog_cpp)
target_include_directories(csv_to_ulog PUBLIC ${rapidcsv_SOURCE_DIR}/src)
