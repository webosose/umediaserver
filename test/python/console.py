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
#

import sys
import readline
import threading
import Queue
import argparse

from uMediaServer.uMediaClient import MediaPlayer

print "uMediaClient OO (python)\n"

# commands:
# load <file/uri>
#   e.g. load file:///home/projects/umediaserver/media_files/rat.mp4
#
# play
# pause
# seek <value (seconds)>
# unload

done = False

def ev_worker(q):
    while True:
        (ev, data) = q.get()
        print "ev '%s' = (%s)" % (ev, data)

def start_ev_wrk(umc):
    q = Queue.Queue()
    umc.setQueue(q)

    ev_handler = threading.Thread(target=ev_worker, args=(q,))
    ev_handler.daemon = True
    ev_handler.start()

arg_parser = argparse.ArgumentParser(description='Command line python tool. similar to umc')
arg_parser.add_argument('--tv', type=bool, default=False, help='enable tv-mode')
args = vars(arg_parser.parse_args())

umc = MediaPlayer()
start_ev_wrk(umc)

while (not done):
    input_command = raw_input("COMMANDS: 'load file:////media_files/rat.mp4', 'play', 'pause', 'unload', 'exit' : ")
    print " ", input_command

    args = input_command.split(" ")

    if args[0] == "load" :
        if len(args) >= 2:
            umc.load(args[1], args[2], args[3])
        else:
            print "load command requires <file/uri>"

    if args[0] == "play":
        umc.play()

    if args[0] == "pause":
        umc.pause()

    if args[0] == "seek":
        position = 0
        if len(args) >= 2:
            position = long(args[1])
            umc.seek(position)
        else:
            position = 0

        umc.seek(position)

    if args[0] == "unload":
        umc.unload()

    if args[0] == "exit":
        done = True

sys.exit()
