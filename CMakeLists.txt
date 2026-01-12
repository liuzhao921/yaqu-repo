cmake_minimum_required(VERSION 3.10)
project(DeviceAccessModule CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include local headers (for mock eXosip2)
include_directories(${CMAKE_SOURCE_DIR}/include)

# Find osip2 (which is installed)
find_package(PkgConfig REQUIRED)
pkg_check_modules(OSIP2 REQUIRED libosip2)

include_directories(${OSIP2_INCLUDE_DIRS})
link_directories(${OSIP2_LIBRARY_DIRS})

add_subdirectory(src)
