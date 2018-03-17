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

echo "Luna Service 2 for uMediaServer version 1.0"

usage() {
    echo "Start or stop a local hub"
    echo "Options:"
    echo "  help            Show this help message"
    echo ""
    echo "Commands:"
    echo "  start_public          Start public hub"
    echo "  start_private         Start private hub"
    echo "  start                 Start both public and private"
    echo "  stop                  Stop all running instances of hub"
}

LS_PID=`ps auwx | grep ${LS2_HUB} | grep -v grep | tr -s [:space:] | cut -f2 -d' '`

stop_all() {
    killall ls-hubd
}


case "$1" in
start_public*)
	${LS2_HUB} -p -c ${LS2_PUB_CONF} &
	;;
start_private*)
	${LS2_HUB} -c ${LS2_PRV_CONF} &
    ;;
start)
	${LS2_HUB} -p -c ${LS2_PUB_CONF} &
	${LS2_HUB} -c ${LS2_PRV_CONF} 
    ;;
stop)
    stop_all
    ;;
*)
    usage
    ;;
esac
