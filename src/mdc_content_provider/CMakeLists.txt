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

message(STATUS "BUILDING mdc-content-provider")

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
set(LIB_CPP_NAME "mdc_content_provider")
set(TARGET_SRCS "MDCContentProvider.cpp")
add_library(${LIB_CPP_NAME} SHARED ${TARGET_SRCS})
set_target_properties(${LIB_CPP_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${LIB_CPP_NAME}
                      ums_connector
                      ums_connector_impl
                      rt
                      ${LIBPBNJSON_LDFLAGS}
                      ${PMLOGLIB_LDFLAGS})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mdc_content_provider.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/${LIB_CPP_NAME}.pc @ONLY)
webos_build_library(NAME ${LIB_CPP_NAME})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${LIB_CPP_NAME}.pc DESTINATION ${WEBOS_INSTALL_PKGCONFIGDIR})
