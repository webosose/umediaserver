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

## ---
# build libraries
set(LIB_MDC_NAME media_display_controller)

if(${USE_AVOUTPUTD})
	add_definitions(-DUSE_AVOUTPUTD)
	set(DISPLAY_CONNECTOR_SRC AVOutputContextlessDisplayConnector.cpp)
endif(${USE_AVOUTPUTD})

if("${WEBOS_TARGET_MACHINE}" STREQUAL "m16")
    add_definitions(-DCONFIG_OVERLAY_MODE)
endif()

set(TARGET_SRCS MediaDisplayController.cpp ${DISPLAY_CONNECTOR_SRC} ConnectionPolicy.cpp
    AppObserver.cpp LayoutManager.cpp media_object.cpp)
add_library(${LIB_MDC_NAME} SHARED ${TARGET_SRCS})
set_target_properties(${LIB_MDC_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries (${LIB_MDC_NAME}
					   ${GLIB2_LDFLAGS}
					   ${LUNASERVICE_LDFLAGS}
					   ${LIBPBNJSON_LDFLAGS}
					   ${PMLOGLIB_LDFLAGS}
					   ums_connector
					   ums_connector_impl)

webos_build_library(NAME ${LIB_MDC_NAME})
