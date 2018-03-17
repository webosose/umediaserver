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

message(STATUS "BUILDING test/python/framework")

set(CONF_UMS_LOG_FILE "${WEBOS_INSTALL_LOGDIR}/ums.log")
configure_file(src/config.py.in src/config.py @ONLY)
configure_file(src/cli_controller.py src/cli_controller.py COPYONLY)
configure_file(src/cli_analyzer.py src/cli_analyzer.py COPYONLY)
configure_file(src/parser/__init__.py src/parser/__init__.py COPYONLY)
configure_file(src/parser/parser.py src/parser/parser.py COPYONLY)
configure_file(src/formatter/__init__.py src/formatter/__init__.py COPYONLY)
configure_file(src/formatter/formatter.py src/formatter/formatter.py COPYONLY)
configure_file(src/detector/__init__.py src/detector/__init__.py COPYONLY)
configure_file(src/detector/detector.py src/detector/detector.py COPYONLY)
configure_file(test/unit/detector.py test/unit/detector.py COPYONLY)
