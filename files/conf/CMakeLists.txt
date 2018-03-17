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

# Default settings used for device. Target-specific overrides are done below.
# If you change a value here, check to see if the overrides below are still necessary.

webos_machine_impl_dep()

set(CONF_UMS_LOG_FILE "${WEBOS_INSTALL_LOGDIR}/ums.log")

webos_build_configured_file(files/conf/fileschema.js SYSCONFDIR umediaserver)
webos_build_configured_file(files/conf/umediaserver.conf SYSCONFDIR pmlog.d)

configure_file(00-ums.conf.in 00-ums.conf @ONLY)
