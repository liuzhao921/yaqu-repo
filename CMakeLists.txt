cmake_minimum_required(VERSION 3.10)
project(DeviceAccessModule CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include local headers (for mock eXosip2)
include_directories(${CMAKE_SOURCE_DIR}/include)

# Find osip2 (which is installed)
find_package(PkgConfig REQUIRED)
pkg_check_modules(OSIP2 REQUIRED libosip2)

# Find libxml2
pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)

# Find FFmpeg libraries
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)

include_directories(${OSIP2_INCLUDE_DIRS})
link_directories(${OSIP2_LIBRARY_DIRS})
include_directories(${LIBXML2_INCLUDE_DIRS})
include_directories(${AVCODEC_INCLUDE_DIRS})
include_directories(${AVFORMAT_INCLUDE_DIRS})
include_directories(${AVUTIL_INCLUDE_DIRS})
include_directories(${SWSCALE_INCLUDE_DIRS})

add_subdirectory(src)
