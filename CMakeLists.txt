# Copyright (c) 2008-2018 LG Electronics, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 2.8.7)
project(umediaserver C CXX)
include(webOS/webOS)
webos_modules_init(1 0 0 QUALIFIER RC4)
webos_component(1 0 0)

include(FindPkgConfig)

# ---
# add local public headers prior to packages
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
webos_add_compiler_flags(ALL -I${CMAKE_SOURCE_DIR}/include/public -I${CMAKE_SOURCE_DIR}/src/resource_manager)
include_directories(${CMAKE_SOURCE_DIR}/include/public)

# ---
#  find required packages

pkg_check_modules(LIBCONFIG REQUIRED libconfig)
webos_add_compiler_flags(ALL ${LIBCONFIG_CFLAGS})

pkg_check_modules(LIBCONFIGPP REQUIRED libconfig++)
webos_add_compiler_flags(ALL ${LIBCONFIGPP_CFLAGS})

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
webos_add_compiler_flags(ALL ${GLIB2_CFLAGS_OTHER})

pkg_check_modules(GTHREAD2 REQUIRED gthread-2.0)
webos_add_compiler_flags(ALL ${GTHREAD2_CFLAGS_OTHER})

pkg_check_modules(LUNASERVICE REQUIRED luna-service2)
webos_add_compiler_flags(ALL ${LUNASERVICE_CFLAGS})

pkg_check_modules(LIBPBNJSON REQUIRED pbnjson_cpp)
webos_add_compiler_flags(ALL ${LIBPBNJSON_CFLAGS})

find_package(Boost REQUIRED COMPONENTS regex filesystem)
include_directories(${Boost_INCLUDE_DIRS})

pkg_check_modules(PMLOGLIB REQUIRED PmLogLib>=3.0)
webos_add_compiler_flags(ALL ${PMLOGLIB_CFLAGS})

pkg_check_modules(PMTRACE PmTrace)
if (PMTRACE_FOUND)
	webos_add_compiler_flags(ALL -DLTTNG_PMTRACE -DTRACEPOINT_DEFINE -DTRACEPOINT_PROBE_DYNAMIC_LINKAGE ${PMTRACE_CFLAGS})
	set(PMTRACE_LDFLAGS -ldl ${PMTRACE_LDFLAGS})
endif ()

if("${WEBOS_TARGET_MACHINE}" STREQUAL "" OR "${WEBOS_TARGET_MACHINE}" MATCHES "raspberrypi*")
    message(STATUS "Using INTERNAL API v.2")
    add_definitions(-DUMS_INTERNAL_API_VERSION=2)
else()
    message(STATUS "Using INTERNAL API v.1")
endif()

if("${WEBOS_TARGET_MACHINE}" MATCHES "raspberrypi*")
    message(STATUS "Using raspberry machine")
    add_definitions(-DUSE_RPI_RESOURCE)
endif()

# ---
#  build components
add_subdirectory(src/logger)
add_subdirectory(src/mdc_client)
add_subdirectory(src/mdc_content_provider)
add_subdirectory(src/utils)
add_subdirectory(src/resource_manager)
add_subdirectory(src/media_display_controller)
add_subdirectory(src/ums_connector)
add_subdirectory(src/pipeline)
add_subdirectory(src/server)
add_subdirectory(src/media_client)
add_subdirectory(src/dbi)
add_subdirectory(src/registry)
add_subdirectory(src/process_pool)
add_subdirectory(files/conf)
add_subdirectory(test/pipelines)
add_subdirectory(test/resource_manager)
add_subdirectory(test/python)
add_subdirectory(test/ums_connector)
add_subdirectory(test/dbi)
add_subdirectory(test/registry)
add_subdirectory(test/media_client)
add_subdirectory(test/process_pool)

# show headers in IDE
file(GLOB_RECURSE UMS_HEADERS "*.h")
add_custom_target(show-ums-headers SOURCES ${UMS_HEADERS})

# webos commands
webos_build_pkgconfig()
webos_build_system_bus_files()

# install the script file
# webos_configure_source_files(UMEDIA-CONTROL files/scripts/public/ls-control)
# install(PROGRAMS ${UMEDIA-CONTROL} DESTINATION ${WEBOS_INSTALL_BINDIR} ${UMEDIA_PERMS})
