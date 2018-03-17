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

message(STATUS "BUILDING server")

include_directories(
                   ${CMAKE_CURRENT_SOURCE_DIR}
                   ${CMAKE_SOURCE_DIR}/src/logger
                   ${CMAKE_SOURCE_DIR}/src/timer
                   ${CMAKE_SOURCE_DIR}/src/utils
                   ${CMAKE_SOURCE_DIR}/src/resource_manager
                   ${CMAKE_SOURCE_DIR}/src/ums_connector
                   ${CMAKE_SOURCE_DIR}/src/pipeline
                   ${CMAKE_SOURCE_DIR}/src/process_pool
                   ${CMAKE_SOURCE_DIR}/src/pipeline_manager
				   ${CMAKE_SOURCE_DIR}/src/power_manager
                   ${CMAKE_SOURCE_DIR}/src/dbi
                   ${CMAKE_SOURCE_DIR}/src/registry
                   ${CMAKE_SOURCE_DIR}/src/media_display_controller
                   )

set(
   LIBS
   rt
   pipeline
   pipeline_mgr
   resource_mgr
   ums_connector
   ums_connector_impl
   ums_registry
   media_display_controller
   power_manager
   boost_system
   boost_filesystem
   
   ${GLIB2_LIBRARIES}
   ${LUNA_SERVICE_LIBRARIES}
   ${LIBPBNJSON_LDFLAGS}
   ${LIBCONFIG_LDFLAGS}
   ${PMTRACE_LDFLAGS}
   ${LUNASERVICE_LDFLAGS}
   ${LIBCONFIGPP_LDFLAGS}
   ${PMLOGLIB_LDFLAGS}
   ${LIBCONFIGPP_LDFLAGS}   
   )

# ---
# create server executable
set(SERVER_NAME "umediaserver")
set(src main.cpp uMediaserver.cpp)
add_executable (${SERVER_NAME} ${src})
set_target_properties(${SERVER_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries (${SERVER_NAME} ${LIBS})

webos_build_daemon(LAUNCH files/launch)
webos_build_system_bus_files()
