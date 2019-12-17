# Copyright (c) 2008-2019 LG Electronics, Inc.
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

include_directories(
                   ${CMAKE_CURRENT_SOURCE_DIR}
                   ${CMAKE_SOURCE_DIR}/src/logger
                   )

# ---
# create test executable
set(BIN_NAME "ums_rmc")
set(test_exe_src rmc.cpp)
add_executable (${BIN_NAME} ${test_exe_src})
set_target_properties(${BIN_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries (${BIN_NAME}
						ums_connector
						ums_connector_impl
						resource_mgr
						resource_mgr_client
						${LIBCONFIG_LDFLAGS}
						${LIBCONFIGPP_LDFLAGS}
						${GTHREAD2_LDFLAGS}
						${PMLOGLIB_LDFLAGS}
						${LIBPBNJSON_LDFLAGS})
install(TARGETS ${BIN_NAME} DESTINATION ${WEBOS_INSTALL_BINDIR})

# ---
# test executable

set(TESTNAME_CPP "umc")
set(src umc.cpp)
add_executable (${TESTNAME_CPP} ${src})
set_target_properties(${TESTNAME_CPP} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries (${TESTNAME_CPP}
                       ${GLIB2_LDFLAGS}
                       ${LUNASERVICE_LDFLAGS}
                       ${LIBPBNJSON_LDFLAGS}
                       ${PMLOGLIB_LDFLAGS}
                       ums_connector
                       ums_connector_impl
                       ums_connector_wrapper
                       rt
                       pthread
                       umedia_api)
install(TARGETS ${TESTNAME_CPP} DESTINATION ${WEBOS_INSTALL_BINDIR})

# ---
# test executable

set(UMS_TOOL_BIN_NAME "ums-status")
set(UMS_TOOL_SRC ums-status.cpp)
add_executable (${UMS_TOOL_BIN_NAME} ${UMS_TOOL_SRC})
set_target_properties(${UMS_TOOL_BIN_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries (${UMS_TOOL_BIN_NAME}
                       ${GLIB2_LDFLAGS}
                       ${LUNASERVICE_LDFLAGS}
                       ${LIBPBNJSON_LDFLAGS}
                       ${PMLOGLIB_LDFLAGS}
                       ums_connector
                       ums_connector_impl
                       ums_connector_wrapper
                       rt)
install(TARGETS ${UMS_TOOL_BIN_NAME} DESTINATION ${WEBOS_INSTALL_BINDIR})

add_subdirectory(ums-status2)

# build Watchdog library
# set(TARGET_NAME watchdog)
# set(TARGET_SRCS "Watchdog.cpp" "Timer.cpp")
# add_library(${TARGET_NAME} STATIC ${TARGET_SRCS})
