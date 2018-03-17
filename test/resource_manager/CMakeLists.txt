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

include_directories(
				${CMAKE_CURRENT_SOURCE_DIR}
				${CMAKE_SOURCE_DIR}/src/logger
				${CMAKE_SOURCE_DIR}/src/timer
				${CMAKE_SOURCE_DIR}/src/utils
				)


# ---
# create test executable
set(BIN_NAME "test_resource_manager")
set(test_exe_src test.cpp)
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


# ---
# create test executable
set(TEST_NAME_UNIT "test_resource_manager_client_unit")
set(test_exe_src test_client_unit.cpp)
add_executable (${TEST_NAME_UNIT} ${test_exe_src})
set_target_properties(${TEST_NAME_UNIT} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries (${TEST_NAME_UNIT}
						ums_connector
						ums_connector_impl
						resource_mgr_client
						${PMLOGLIB_LDFLAGS}
						${LIBPBNJSON_LDFLAGS})

# ---
# create test executable
add_executable (RM_test RM_test.cpp)
target_link_libraries (RM_test
                       ums_connector
                       resource_mgr_client
                       resource_mgr
                       ${LIBPBNJSON_LDFLAGS})
