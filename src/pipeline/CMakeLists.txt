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

message(STATUS "BUILDING ums_pipeline")

include_directories(
                   ${CMAKE_CURRENT_SOURCE_DIR}
                   ${CMAKE_SOURCE_DIR}/src/logger
                   ${CMAKE_SOURCE_DIR}/src/timer
                   ${CMAKE_SOURCE_DIR}/src/config_file_reader
                   ${CMAKE_SOURCE_DIR}/src/server
                   ${CMAKE_SOURCE_DIR}/src/utils
                   ${CMAKE_SOURCE_DIR}/src/ums_connector
                   ${CMAKE_SOURCE_DIR}/src/process_pool
                   ${CMAKE_SOURCE_DIR}/src/dbi
                   ${CMAKE_SOURCE_DIR}/src/registry
                   )

set(
   LINK_DEPS
   pthread
   rt
   resource_mgr
   ums_connector
   ums_connector_impl
   process_pool
   ${GLIB2_LDFLAGS}
   ${LIBPBNJSON_LDFLAGS}
   ${LUNASERVICE_LDFLAGS}
   ${LIBCONFIG_LDFLAGS}
   )

## ---
# build libraries
set(LIB_PIPELINE_NAME "ums_pipeline")
set(TARGET_SRCS "Pipeline.cpp")
add_library(${LIB_PIPELINE_NAME} SHARED ${TARGET_SRCS})
# confirming link language here avoids linker confusion and prevents errors seen previously
set_target_properties(${LIB_PIPELINE_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries (${LIB_PIPELINE_NAME} ${LINK_DEPS})
webos_build_library(NAME ${LIB_PIPELINE_NAME})

set(LIB_PIPELINE_MGR_NAME "ums_pipeline_mgr")
set(TARGET_SRCS "PipelineManager.cpp")
add_library(${LIB_PIPELINE_MGR_NAME} SHARED ${TARGET_SRCS})
target_link_libraries(${LIB_PIPELINE_MGR_NAME} ums_pipeline)
webos_build_library(NAME ${LIB_PIPELINE_MGR_NAME})
