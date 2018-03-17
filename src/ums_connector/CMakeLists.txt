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

message(STATUS "BUILDING ums_connector")

include_directories(
                   ${CMAKE_CURRENT_SOURCE_DIR}
                   ${CMAKE_SOURCE_DIR}/src/logger
                   ${CMAKE_SOURCE_DIR}/src/timer
                   ${CMAKE_SOURCE_DIR}/src/utils
                   ${CMAKE_SOURCE_DIR}/src/pipeline
                   )

# Implementation library
set(LIB_IMPL_NAME "ums_connector_impl")
set(TARGET_SRCS "UMSConnector_impl.cpp")
add_library(${LIB_IMPL_NAME} SHARED ${TARGET_SRCS})
# confirming link language here avoids linker confusion and prevents errors seen previously
set_target_properties(${LIB_IMPL_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${LIB_IMPL_NAME}
                      ${GLIB2_LDFLAGS}
                      ${LUNASERVICE_LDFLAGS}
                      ${LIBPBNJSON_LDFLAGS}
                      ${PMLOGLIB_LDFLAGS}
                      ums_connector_impl)
webos_build_library(NAME ${LIB_IMPL_NAME})

# Interface library
set(LIB_NAME "ums_connector")
set(TARGET_SRCS "UMSConnector.cpp" "CallbackManager.cpp")
add_library(${LIB_NAME} SHARED ${TARGET_SRCS})
set_target_properties(${LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${LIB_NAME}
                      ${GLIB2_LDFLAGS}
                      ${LUNASERVICE_LDFLAGS}
                      ${LIBPBNJSON_LDFLAGS}
                      ${PMLOGLIB_LDFLAGS}
                      ums_connector_impl)
webos_build_library(NAME ${LIB_NAME})

# Wrapper
set(LIB_WRAPPER_NAME "ums_connector_wrapper")
set(TARGET_SRCS "UMSConnector_wrapper.cpp")
add_library(${LIB_WRAPPER_NAME} SHARED ${TARGET_SRCS})
# confirming link language here avoids linker confusion and prevents errors seen previously
set_target_properties(${LIB_WRAPPER_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${LIB_WRAPPER_NAME}
                      ums_connector
                      ums_connector_impl
                      ${LIBPBNJSON_LDFLAGS}
                      ${PMLOGLIB_LDFLAGS})
webos_build_library(NAME ${LIB_WRAPPER_NAME})
