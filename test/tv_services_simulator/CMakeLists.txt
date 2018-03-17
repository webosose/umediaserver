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

message(STATUS "BUILDING Media Display Controller")

include_directories(
                   ${CMAKE_CURRENT_SOURCE_DIR}
                   ${CMAKE_SOURCE_DIR}/src/logger
                   ${CMAKE_SOURCE_DIR}/src/server
                   ${CMAKE_SOURCE_DIR}/src/ums_connector
                   ${CMAKE_SOURCE_DIR}/src/utils
                   )


set(
   LIBS
   rt
   ums_connector
   ums_connector_impl
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
   ${LIBCONFIGPP_LDFLAGS})

# ---
# create TV Services Simulator Server
set(SERVER_NAME "tvservices")
set(src main.cpp)
add_executable (${SERVER_NAME} ${src})
set_target_properties(${SERVER_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries (${SERVER_NAME} ${LIBS})














