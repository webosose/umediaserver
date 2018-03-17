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
#

include(FindPkgConfig)

pkg_check_modules(SQLITE3 REQUIRED sqlite3>=3.7.15)

message(STATUS "BUILDING ums_dbi")

include_directories(${SQLITE3_INCLUDE_DIRS})
link_directories(${SQLITE3_LIBRARY_DIRS})

file(GLOB_RECURSE SOURCES "*.cpp")
add_library(ums_dbi SHARED ${SOURCES})
target_link_libraries(ums_dbi ${SQLITE3_LIBRARIES})
webos_build_library(NAME ums_dbi)
