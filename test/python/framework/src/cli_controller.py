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

from parser import *
from formatter import *

try:
    from config import *
except ImportError as e:
    sys.stderr.write('Unable to import config - using default settings\n')
    UMS_LOG_FILE='ums.log'

arg_parser = argparse.ArgumentParser(description='Process uMediaServer log output')
arg_parser.add_argument('-f', '--file',
                    action ='store',
                    default = UMS_LOG_FILE,
                    help ='log file to process')
arg_parser.add_argument('-s', '--session',
                    action ='store',
                    help ='session id to filter')
arg_parser.add_argument('-p', '--pipeline',
                    action ='store',
                    help ='pipeline id to filter')
arg_parser.add_argument('-l', '--list',
                    action ='store_true',
                    default =False,
                    help ='list all sessions and pipelines')
arg_parser.add_argument('-o', '--format',
                    action ='store',
                    choices =['raw', 'csv', 'json'],
                    default ='raw',
                    help='output format')

args = vars(arg_parser.parse_args())

if args['format'] == 'raw':
    args['format'] = Format.RAW
elif args['format'] == 'csv':
    args['format'] = Format.CSV
elif args['format'] == 'json':
    args['format'] = Format.JSON

try:
    log_file = open(args['file'])
except IOError as e:
    sys.stderr.write('Unable to open log file: ' + e.strerror + '\n')
    exit(-1)

session_starter = 'Starting client session'
session_binder = 'Subscribed to notifications from: '

def list_sessions(log_file):
    sessions = dict()

    for line in log_file:
        try:
            parsed = Parser.parse(line)
        except Exception as e:
            sys.stderr.write(e.args[0] + '\n')
            continue
        session = parsed['msg']['info']['SESSION_ID'] if 'SESSION_ID' in parsed['msg']['info'] else ''
        # do we have new session?
        if parsed['msg']['message'] == session_starter:
            sessions[session] = set()
        # do we have pipeline binding?
        elif session_binder in parsed['msg']['message']:
            sessions[session].add(parsed['msg']['message'][len(session_binder):])

    for sid in sessions:
        print('session: ' + sid)
        for pid in sessions[sid]:
            print('\tpipeline: ' + pid)

def filter_log(log_file):
    # session => { pipelines : [], messages : [] }
    sessions = dict()
    # pipeline => { session : session_id, messages : [] }
    pipelines = dict()
    # all messages
    unfiltered = []

    def compare_by_timestamp(a, b):
        return float(a['msg']['info']['TIMESTAMP']) < float(a['msg']['info']['TIMESTAMP'])

    def merge_pipeline(pipeline_id, session_id):
        session = sessions[session_id]
        pipeline = pipelines[pipeline_id]
        pipeline_idx = 0
        session_idx = 0
        while(pipeline_idx < len(pipeline['messages'])):
            while(session_idx < len(session['messages']) and\
                  compare_by_timestamp(session['messages'][session_idx], pipeline['messages'][pipeline_idx])):
                session_idx += 1
            session['messages'].insert(session_idx, pipeline['messages'][pipeline_idx])
            pipeline_idx += 1

    def process_log_line(line):
        try:
            parsed = Parser.parse(line)
        except Exception as e:
            sys.stderr.write(e.args[0])
            return
        unfiltered.append(parsed)
        id = parsed['msg']['info']['SESSION_ID'] if 'SESSION_ID' in parsed['msg']['info'] else ''
        if id:
            id = parsed['msg']['info']['SESSION_ID']
            msg = parsed['msg']['message']
            # new session started
            if msg == session_starter:
                session = {'pipelines' : [], 'messages' : []}
                session['messages'].append(parsed)
                sessions[id] = session
            # pipeline to session binding
            elif session_binder in msg:
                pipeline_id = msg[len(session_binder):]
                pipeline = pipelines[pipeline_id]
                pipeline['session'] = id
                pipeline['messages'].append(parsed)
                merge_pipeline(pipeline_id, id)
            # client message
            elif id in sessions:
                sessions[id]['messages'].append(parsed)
            # pipeline message
            elif id in pipelines:
                pipeline = pipelines[id]
                pipeline['messages'].append(parsed)
                session_id = pipeline['session']
                if session_id:
                    session = sessions[session_id]
                    session['messages'].append(parsed)
            # new pipeline came out
            else:
                pipelines[id] = {'session' : None, 'messages' : [parsed]}

    def print_log():
        if args['pipeline'] in pipelines:
            pipeline = pipelines[args['pipeline']]
            for parsed in pipeline['messages']:
                print(Formatter.format(parsed, args['format']);)
        elif args['session'] in sessions:
            session = sessions[args['session']]
            for parsed in session['messages']:
                print(Formatter.format(parsed, args['format']);)
        else:
            for parsed in unfiltered:
                 print(Formatter.format(parsed, args['format']);)

    for line in log_file:
        process_log_line(line)

    print_log()

if args['list']:
    list_sessions(log_file)
else:
    filter_log(log_file)
