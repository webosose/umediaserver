#!/bin/bash
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

# directories
LS2_BASE_DIR="/usr/local"
LS2_BIN_DIR="${LS2_BASE_DIR}/bin"
LS2_LIB_DIR="${LS2_BASE_DIR}/lib"
LS2_CONF_DIR=${LS2_BASE_DIR}/etc/ls2
LS2_ROLES_DIR=${LS2_BASE_DIR}/usr/share/ls2/roles
LS2_SERVICES_DIR=${LS2_BASE_DIR}/var/palm/services
LS2_SYSTEM_SERVICES_DIR=${LS2_BASE_DIR}/var/palm/system-services

# system configuration files
LS2_PUB_CONF=${LS2_BASE_DIR}/etc/ls2/ls-public.conf
LS2_PRV_CONF=${LS2_BASE_DIR}/etc/ls2/ls-private.conf

# luna service 2 hub binary
LS2_HUB="${LS2_BIN_DIR}/ls-hubd"
LS2_MONITOR="${LS2_BIN_DIR}/ls-monitor"
LS2_CONTROL="${LS2_BIN_DIR}/ls-control"

export LD_LIBRARY_PATH=${LS2_LIB_DIR}:${LS2_BIN_DIR}:${LD_LIBRARY_PATH}

echo "-----------------------------------"
echo "Luna Service 2 configuration :"
echo "LS2_BASE_DIR            = ${LS2_BIN_DIR}"
echo "LS2_BIN_DIR             = ${LS2_BIN_DIR}"
echo "LS2_LIB_DIR             = ${LS2_LIB_DIR}"
echo "LS2_CONF_DIR            = ${LS2_CONF_DIR}"
echo "LS2_ROLES_DIR           = ${LS2_ROLES_DIR}"
echo "LS2_SERVICES_DIR        = ${LS2_SERVICES_DIR}"
echo "LS2_SYSTEM_SERVICES_DIR = ${LS2_SYSTEM_SERVICES_DIR}"
echo "LS2_PUB_CONF            = ${LS2_PUB_CONF}"
echo "LS2_PRV_CONF            = ${LS2_PRV_CONF}"
echo "LS2_HUB                 = ${LS2_HUB}"
echo "LS2_MONITOR             = ${LS2_MONITOR}"
echo "LS2_CONTROL             = ${LS2_CONTROL}"
echo "-----------------------------------"


