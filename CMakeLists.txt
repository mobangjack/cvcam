cmake_minimum_required(VERSION 2.8)

project(cvcam)

find_package(OpenCV REQUIRED)

add_executable(cvcam src/cvcam.cpp)
target_link_libraries(cvcam ${OpenCV_LIBRARIES})
