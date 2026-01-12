cmake_minimum_required(VERSION 3.10)
project(DeviceAccessModule CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(src)

# Find and link necessary libraries (e.g., Boost, OpenCV, etc.)
# find_package(Boost REQUIRED COMPONENTS system thread)
# target_link_libraries(DeviceAccessModule PRIVATE Boost::system Boost::thread)

# Example for a simple web server library (e.g., Crow, Civetweb)
# find_package(Civetweb REQUIRED)
# target_link_libraries(DeviceAccessModule PRIVATE Civetweb::Civetweb)
