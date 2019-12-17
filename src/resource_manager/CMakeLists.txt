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

message(STATUS "BUILDING resource_manager")

include_directories(
				${CMAKE_CURRENT_SOURCE_DIR}
				${CMAKE_SOURCE_DIR}/src/logger
				${CMAKE_SOURCE_DIR}/src/timer
				${CMAKE_SOURCE_DIR}/src/config_file_reader
				${CMAKE_SOURCE_DIR}/src/utils
				)

# Resource Manager library
set(RESMGR_LIB_NAME "resource_mgr")
set(TARGET_SRCS ResourceManager.cpp AppLifeManager.cpp AcquireQueue.cpp)
add_library(${RESMGR_LIB_NAME} SHARED ${TARGET_SRCS})
set_target_properties(${RESMGR_LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${RESMGR_LIB_NAME}
						boost_regex
						${LIBCONFIG_LDFLAGS}
						${LIBCONFIGPP_LDFLAGS}
						${GTHREAD2_LDFLAGS}
						${PMLOGLIB_LDFLAGS}
						${LIBPBNJSON_LDFLAGS}
						rt)
webos_build_library(NAME ${RESMGR_LIB_NAME})

# Resource Manager Client library C++
set(RESMGR_CLIENT_LIB_CXX_NAME "resource_mgr_client")
set(TARGET_SRCS ResourceManagerClient.cpp)
add_library(${RESMGR_CLIENT_LIB_CXX_NAME} SHARED ${TARGET_SRCS})
set_target_properties(${RESMGR_CLIENT_LIB_CXX_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${RESMGR_CLIENT_LIB_CXX_NAME}
						${LIBCONFIG_LDFLAGS}
						${LIBCONFIGPP_LDFLAGS}
						${GTHREAD2_LDFLAGS}
						${PMLOGLIB_LDFLAGS}
						${LIBPBNJSON_LDFLAGS}
						rt
						ums_connector
						ums_connector_impl
						pthread)
webos_build_library(NAME ${RESMGR_CLIENT_LIB_CXX_NAME})

# Resource Manager Client library C
set(RESMGR_CLIENT_LIB_C_NAME "resource_mgr_client_c")
set(TARGET_SRCS ResourceManagerClient_c.cpp)
add_library(${RESMGR_CLIENT_LIB_C_NAME} SHARED ${TARGET_SRCS})
set_target_properties(${RESMGR_CLIENT_LIB_C_NAME} PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(${RESMGR_CLIENT_LIB_C_NAME}
						${LIBCONFIG_LDFLAGS}
						${LIBCONFIGPP_LDFLAGS}
						${GTHREAD2_LDFLAGS}
						${PMLOGLIB_LDFLAGS}
						${LIBPBNJSON_LDFLAGS}
						rt
						ums_connector
						ums_connector_impl
						ums_connector_wrapper
						resource_mgr_client
						pthread)
webos_build_library(NAME ${RESMGR_CLIENT_LIB_C_NAME})
