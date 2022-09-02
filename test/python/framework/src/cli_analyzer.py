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

import sys
import argparse
from detector import Detector, LSMessage
from parser import Parser

arg_parser = argparse.ArgumentParser(description='Collect performance data from uMediaServer log output')
arg_parser.add_argument('-f', '--file',
                    action ='store',
                    default = None,
                    help ='log file to process')

args = vars(arg_parser.parse_args())

log_file = sys.stdin

if args['file']:
    try:
        log_file = open(args['file'])
    except IOError as e:
        sys.stderr.write('Unable to open log file: ' + e.strerror + '\n')
        exit(-1)

detector = Detector()

for line in log_file:
    try:
        detector.process(Parser.parse(line))
    except LSMessage.ParseException as e:
        sys.stderr.write(e.args[0] + '\n')
detector.finalize()
print(detector.summary())
