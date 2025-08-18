# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set( CMAKE_CXX_STANDARD 20 )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(button)

target_sources(app PRIVATE src/main.cpp src/seg7.cpp)
set_target_properties(app PROPERTIES CXX_STANDARD 20 )
