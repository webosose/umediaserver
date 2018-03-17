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

message(STATUS "BUILDING process_pool")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}
                     ${CMAKE_SOURCE_DIR}/src/logger
                     ${CMAKE_SOURCE_DIR}/src/utils
                     ${CMAKE_SOURCE_DIR}/src/dbi
                     ${CMAKE_SOURCE_DIR}/src/registry )

# build library
set(LIB_NAME process_pool)
set(TARGET_SRCS Process.cpp ProcessPool.cpp)
add_library(${LIB_NAME} SHARED ${TARGET_SRCS})

# confirming link language here avoids linker confusion and prevents errors seen previously
set_target_properties(${LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${LIB_NAME} ${GLIB2_LDFLAGS} ums_dbi ums_registry)
webos_build_library(NAME ${LIB_NAME})
