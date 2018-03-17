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
                   ${CMAKE_SOURCE_DIR}/src/pipeline
                   )

# ---
# create test executable
set(BIN_NAME "test_ums_connector")
set(test_exe_src test.cpp)
add_executable (${BIN_NAME} ${test_exe_src})
# confirming link language here avoids linker confusion and prevents errors seen previously
set_target_properties(${BIN_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries (${BIN_NAME}
                       ums_connector
                       ums_connector_impl
                       rt
                       ${GLIB2_LIBRARIES}
                       ${LIBPBNJSON_LDFLAGS}
                       ${LUNASERVICE_LDFLAGS}
                       ${LIBCONFIG_LDFLAGS}
                       ${PMLOGLIB_LDFLAGS})
