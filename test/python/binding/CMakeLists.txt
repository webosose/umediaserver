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

message(STATUS "BUILDING test/python/binding")

## ---
# Build other language bindings using SWIG
FIND_PACKAGE(SWIG REQUIRED)
INCLUDE(${SWIG_USE_FILE})

set(PYTHON_NAMESPACE "uMediaServer")
set(PYTHON_MODULE "uMediaClient.py")

FIND_PACKAGE(PythonInterp 2.7)
FIND_PACKAGE(PythonLibs 2.7)
INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/utils)
SET(CMAKE_SWIG_FLAGS "")

SET_SOURCE_FILES_PROPERTIES(uMediaClient.i PROPERTIES CPLUSPLUS ON)

SWIG_ADD_MODULE(uMediaClient python uMediaClient.i)
SWIG_LINK_LIBRARIES(uMediaClient umedia_api ${PYTHON_LIBRARIES})

set(PYTHON_DST ${WEBOS_INSTALL_LIBDIR}/python${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}/site-packages/${PYTHON_NAMESPACE})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${PYTHON_NAMESPACE}/           DESTINATION ${PYTHON_DST})
install(FILES     ${CMAKE_BINARY_DIR}/test/python/binding/${PYTHON_MODULE} DESTINATION ${PYTHON_DST})
install(TARGETS   ${SWIG_MODULE_uMediaClient_REAL_NAME}                      DESTINATION ${PYTHON_DST})
