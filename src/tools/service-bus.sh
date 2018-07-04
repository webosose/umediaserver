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

usage() {
  cat <<-EOF
	Starts or stops local (ls2) ls-hubd sessions
	Usage: ${0} [--help] [command] [args...]
	Options:
	    --help    Show this help message

	Commands:
	    start                      Starts the pub/priv ls-hubd instances
	    stop                       Stops currently running ls-hubd and services
	    monitor [...]              Invokes ls-monitor
	EOF
	echo
}

hubd_stop() {
  kill `pidof ls-hubd`
}

hubd_start() {
  ${USR_SBIN_DIR}/ls-hubd --conf ${CONF_DIR}/ls-hubd.conf 2>&1 &
  sleep 1
  echo
  echo "hub daemons started!"
  echo "(Type: '${0} stop' to stop hub daemons.)"
}

hubd_monitor() {
  ls-monitor "$@"
}

#########################################################

ROOTFS=/usr/local/webos
LIB_DIR=${ROOTFS}/lib
USR_LIB_DIR=${ROOTFS}/usr/lib
USR_BIN_DIR="${ROOTFS}/usr/bin"
USR_SBIN_DIR="${ROOTFS}/usr/sbin"
CONF_DIR="${ROOTFS}/etc/luna-service2"

export LD_LIBRARY_PATH=${LIB_DIR}:${USR_LIB_DIR}:${LD_LIBRARY_PATH}
export PATH=${USR_BIN_DIR}:${USR_SBIN_DIR}:${BIN_DIR}:${PATH}

CMD="$1"
if [ -z "$CMD" ]; then
  CMD=help
else
  shift
fi

echo

case "$CMD" in
start)
  echo "Halting old services..."
  hubd_stop
  echo
  hubd_start
  echo
  ;;
stop)
  echo "Halting services..."
  hubd_stop
  ;;
monitor)
  hubd_monitor ;;
help)
  usage ;;
*)
  usage && false ;;
esac
