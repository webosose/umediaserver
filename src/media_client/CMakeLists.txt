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

message(STATUS "BUILDING umedia-api")

include_directories(
                   ${CMAKE_CURRENT_SOURCE_DIR}
                   ${CMAKE_SOURCE_DIR}/src/logger
                   ${CMAKE_SOURCE_DIR}/src/utils
                   ${CMAKE_SOURCE_DIR}/src/ums_connector
                   ${CMAKE_SOURCE_DIR}/src/media_client
                   )

if(NOT DEFINED ${prefix})
set(prefix      ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX})
set(libdir      ${CMAKE_INSTALL_PREFIX}/lib)
set(includedir  ${CMAKE_INSTALL_PREFIX}/usr/include)
endif()

# build C++ library
# note: not naming c++ API "cpp" for backwards capability with existing
# projects using this library
set(LIB_CPP_NAME "umedia_api")
set(TARGET_SRCS "uMediaClient.cpp")
add_library(${LIB_CPP_NAME} SHARED ${TARGET_SRCS})
set_target_properties(${LIB_CPP_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${LIB_CPP_NAME}
                      ums_connector
                      ums_connector_impl
                      ${GLIB2_LDFLAGS}
                      ${LIBPBNJSON_LDFLAGS}
                      ${PMLOGLIB_LDFLAGS})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/umedia_api.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/${LIB_CPP_NAME}.pc @ONLY)
webos_build_library(NAME ${LIB_CPP_NAME})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${LIB_CPP_NAME}.pc DESTINATION ${WEBOS_INSTALL_PKGCONFIGDIR})

# build C library
set(LIB_C_NAME "umedia_api_c")
set(TARGET_SRCS "uMediaClient_wrapper.cpp")
add_library(${LIB_C_NAME} SHARED ${TARGET_SRCS})
set_target_properties(${LIB_C_NAME} PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(${LIB_C_NAME}
                      ums_connector
                      ums_connector_impl
                      ums_connector_wrapper
                      umedia_api
                      ${LIBPBNJSON_LDFLAGS}
                      ${PMLOGLIB_LDFLAGS})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/umedia_api_c.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/${LIB_C_NAME}.pc @ONLY)
webos_build_library(NAME ${LIB_C_NAME})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${LIB_C_NAME}.pc DESTINATION ${WEBOS_INSTALL_PKGCONFIGDIR})
