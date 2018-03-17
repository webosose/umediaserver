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

source $(dirname $0)/luna_service2_config.sh

UMS_BUILD_DIR=../BUILD
UMS_LIB_DIR=../lib
UMS_LS2_CONF_DIR=../config/luna-service2
UMS_CONF_DIR=../config/server

UMS_INSTALL_BIN_DIR=/usr/local/bin
UMS_INSTALL_LIB_DIR=/usr/local/lib

export LD_LIBRARY_PATH=${LS2_LIB_DIR}:${LS2_BIN_DIR}:${LD_LIBRARY_PATH}

echo "-----------------------------------"
echo "uMediaServer configuration :"
echo "UMS_BUILD_DIR            = ${UMS_BUILD_DIR}"
echo "UMS_LIB_DIR              = ${UMS_LIB_DIR}"
echo "UMS_LS2_CONF_DIR         = ${UMS_LS2_CONF_DIR}"
echo "UMS_CONF_DIR             = ${UMS_CONF_DIR}"
echo "UMS_INSTALL_BIN_DIR      = ${UMS_INSTALL_BIN_DIR}"
echo "UMS_INSTALL_LIB_DIR      = ${UMS_INSTALL_LIB_DIR}"

#--- make required ls2 target directories

echo " mkdir -p ${LS2_CONF_DIR}"
sudo mkdir -p ${LS2_CONF_DIR}

echo " mkdir -p ${LS2_ROLES_DIR}"
sudo mkdir -p ${LS2_ROLES_DIR}

echo " mkdir -p ${LS2_SERVICES_DIR}"
sudo mkdir -p ${LS2_SERVICES_DIR}

echo " mkdir -p ${LS2_SYSTEM_SERVICES_DIR}"
sudo mkdir -p ${LS2_SYSTEM_SERVICES_DIR}

#--- copy the master configs for LS2
echo " cp ${UMS_LS2_CONF_DIR}/ls-private.conf ${LS2_CONF_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/ls-private.conf ${LS2_CONF_DIR}

echo " cp ${UMS_LS2_CONF_DIR}/ls-public.conf ${LS2_CONF_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/ls-public.conf ${LS2_CONF_DIR}

echo "+++Server"
echo " cp ${UMS_BUILD_DIR}/src/server/umediaserver ${UMS_INSTALL_BIN_DIR}"
sudo cp ${UMS_BUILD_DIR}/src/server/umediaserver ${UMS_INSTALL_BIN_DIR}

echo "+++ ResourceManager configuration file"
echo "cp ${UMS_CONF_DIR}/umediaserver_resource_config.txt /etc/"
sudo cp ${UMS_CONF_DIR}/umediaserver_resource_config.txt /etc

echo "+++Pipeline"
echo " cp ${UMS_BUILD_DIR}/src/test/pipeline/umediapipeline ${UMS_INSTALL_BIN_DIR}/umediapipeline"
sudo cp ${UMS_BUILD_DIR}/src/test/pipeline/umediapipeline ${UMS_INSTALL_BIN_DIR}/umediapipeline

echo "+++ Libraries"
echo " cp ${UMS_BUILD_DIR}/src/resource_manager/libresource_mgr.so ${UMS_INSTALL_LIB_DIR}"
sudo cp ${UMS_BUILD_DIR}/src/resource_manager/libresource_mgr.so ${UMS_INSTALL_LIB_DIR}.

echo " cp ${UMS_BUILD_DIR}/src/pipeline/libpipeline_mgr.so ${UMS_INSTALL_LIB_DIR}"
sudo cp ${UMS_BUILD_DIR}/src/pipeline/libpipeline_mgr.so ${UMS_INSTALL_LIB_DIR}

echo " cp ${UMS_BUILD_DIR}/src/pipeline/libpipeline.so ${UMS_INSTALL_LIB_DIR}"
sudo cp ${UMS_BUILD_DIR}/src/pipeline/libpipeline.so ${UMS_INSTALL_LIB_DIR}

echo " cp ${UMS_BUILD_DIR}/src/ums_connector/libums_connector.so ${UMS_INSTALL_LIB_DIR}"
sudo cp ${UMS_BUILD_DIR}/src/ums_connector/libums_connector.so ${UMS_INSTALL_LIB_DIR}

echo " cp ${UMS_BUILD_DIR}/src/ums_connector/libums_connector_impl.so ${UMS_INSTALL_LIB_DIR}"
sudo cp ${UMS_BUILD_DIR}/src/ums_connector/libums_connector_impl.so ${UMS_INSTALL_LIB_DIR}

echo " cp ${UMS_BUILD_DIR}/src/ums_connector/libums_connector_wrapper.so ${UMS_INSTALL_LIB_DIR}"
sudo cp ${UMS_BUILD_DIR}/src/ums_connector/libums_connector_wrapper.so ${UMS_INSTALL_LIB_DIR}

echo "+++ Install Luna Service configuration files"

# install luna security files for umedia_pipeline process
echo "cp ${UMS_LS2_CONF_DIR}/com.palm.lunasend.json ${LS2_ROLES_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/com.palm.lunasend.json ${LS2_ROLES_DIR}

echo "cp ${UMS_LS2_CONF_DIR}/com.palm.umediapipeline.json ${LS2_ROLES_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/com.palm.umediapipeline.json ${LS2_ROLES_DIR}

echo "cp ${UMS_LS2_CONF_DIR}/com.webos.media.json ${LS2_ROLES_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/com.webos.media.json ${LS2_ROLES_DIR}

echo "cp ${UMS_LS2_CONF_DIR}/ls-monitor.json ${LS2_ROLES_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/ls-monitor.json ${LS2_ROLES_DIR}


# install luna security files for umediaserver process

echo "cp ${UMS_LS2_CONF_DIR}/com.palm.umediapipeline.service ${LS2_SERVICES_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/com.palm.umediapipeline.service ${LS2_SERVICES_DIR}

echo "cp ${UMS_LS2_CONF_DIR}/com.palm.umediapipeline.service ${LS2_SYSTEM_SERVICES_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/com.palm.umediapipeline.service ${LS2_SYSTEM_SERVICES_DIR}

echo "cp ${UMS_LS2_CONF_DIR}/com.palm.umediapipelinectrl.service ${LS2_SERVICES_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/com.palm.umediapipelinectrl.service ${LS2_SERVICES_DIR}

echo "cp ${UMS_LS2_CONF_DIR}/com.palm.umediapipelinectrl.service ${LS2_SYSTEM_SERVICES_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/com.palm.umediapipelinectrl.service ${LS2_SYSTEM_SERVICES_DIR}

echo "cp ${UMS_LS2_CONF_DIR}/com.webos.media.service ${LS2_SERVICES_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/com.webos.media.service ${LS2_SERVICES_DIR}

echo "cp ${UMS_LS2_CONF_DIR}/com.webos.media.service ${LS2_SYSTEM_SERVICES_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/com.webos.media.service ${LS2_SYSTEM_SERVICES_DIR}

echo "cp ${UMS_LS2_CONF_DIR}/com.palm.umediapipeline.json ${LS2_ROLES_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/com.palm.umediapipeline.json ${LS2_ROLES_DIR}

echo "cp ${UMS_LS2_CONF_DIR}/com.palm.umediapipelinectrl.json ${LS2_ROLES_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/com.palm.umediapipelinectrl.json ${LS2_ROLES_DIR}

echo "cp ${UMS_LS2_CONF_DIR}/com.webos.media.json ${LS2_ROLES_DIR}"
sudo cp ${UMS_LS2_CONF_DIR}/com.webos.media.json ${LS2_ROLES_DIR}

