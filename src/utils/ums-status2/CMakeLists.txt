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

message(STATUS "BUILDING ums-status2")

configure_file(start-ums-status.sh.in start-ums-status.sh @ONLY)
configure_file(ums-status-server.js ums-status-server.js COPYONLY)
configure_file(ums-status-client.js ums-status-client.js COPYONLY)
configure_file(ums-status.html ums-status.html COPYONLY)

install(DIRECTORY ./
        DESTINATION ${WEBOS_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}/ums-status
        FILES_MATCHING PATTERN "*.js")
install(DIRECTORY ./
        DESTINATION ${WEBOS_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}/ums-status
        FILES_MATCHING PATTERN "*.html")
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/start-ums-status.sh
        DESTINATION ${WEBOS_INSTALL_PREFIX}/share/${CMAKE_PROJECT_NAME}/ums-status)
