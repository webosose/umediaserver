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

import json
import re

class Process:
    __name_extract = re.compile(r'[^[]*')
    @staticmethod
    def get_name(tag):
        return Process.__name_extract.search(tag).group(0)
    def __init__(self, tag):
        self.name = Process.get_name(tag)

class ProcessInstance:
    __pid_extract = re.compile(r'\[\d+')
    @staticmethod
    def get_pid(tag):
        try:
            return int(ProcessInstance.__pid_extract.search(tag).group(0)[1:])
        except:
            return None
    def __init__(self, tag):
        self.name = Process.get_name(tag)
        self.pid = ProcessInstance.get_pid(tag)
    def __repr__(self):
        return self.name + '[' + str(self.pid) + ']'
    def __str__(self):
        return self.__repr__()

class LSMessage():
    class Direction:
        UNKNOWN, INCOMMING, OUTGOING = range(3)
    class ParseException(Exception):
        pass
    __patterns = {
        'method'  : re.compile(r'method: \w+'),
        'token'   : re.compile(r'token: \w+'),
        'payload' : re.compile(r'body: .+')
    }
    @staticmethod
    def parse_message(msg):
        try:
            if isinstance(msg, LSMessage):
                return msg
            def parse_direction(msg):
                if msg[0:3] == 'rx:':
                    return LSMessage.Direction.INCOMMING
                if msg[0:3] == 'tx:':
                    return LSMessage.Direction.OUTGOING
                return None
            def parse_part(msg, pattern):
                match = pattern.search(msg)
                return match.group(0).split(' ', 1)[1] if match else None
            direction = parse_direction(msg)
            if not direction:
                raise LSMessage.ParseException('LSMessage parsing failed (direction): ' + msg)
            method = parse_part(msg, LSMessage.__patterns['method'])
            token = parse_part(msg, LSMessage.__patterns['token'])
            payload = parse_part(msg, LSMessage.__patterns['payload'])
            if not payload:
                raise LSMessage.ParseException('LSMessage parsing failed (payload): ' + msg)
            try:
                payload = json.loads(payload)
            except:
                payload = {'data' : payload}
            return LSMessage(direction, method, token, payload)
        except LSMessage.ParseException:
            return None

    def __init__(self, direction = None, method = None, token = None, payload = None):
        self.direction = direction
        self.method = method
        self.token = token
        self.payload = payload

class Stage:
    def __init__(self, start, end, processes, name = None):
        self.name = name if name else 'busy'
        self.start = start
        self.end = end
        def calc_elapsed():
            if self.start == None and self.end == None :
                return 0
            return 1.0e3 * (end['msg']['info']['TIMESTAMP'] - start['msg']['info']['TIMESTAMP'])
        self.elapsed = calc_elapsed()
        def get_proc_instance():
            # luna messages start => tx; end => rx
            smsg = start['msg']['message']
            emsg = end['msg']['message']
            if (isinstance(smsg, LSMessage) and isinstance(emsg, LSMessage) and
                smsg.direction == LSMessage.Direction.OUTGOING and
                emsg.direction == LSMessage.Direction.INCOMMING):
                self.name = 'bus call'
                return processes['luna']
            tag = end['syslog']['bsdtag']
            name = Process.get_name(tag)
            if name not in processes:
                processes[name] = ProcessInstance(tag)
            return processes[name]
        self.process = get_proc_instance() if self.start else ProcessInstance('none[0]')
    def __repr__(self):
        return str(self.process) + ': ' + self.name + ': %0.6f' % self.elapsed
    def __str__(self):
        return self.__repr__()

class Sequence(object):
    METHOD = 'unknown'
    class Kind:
        UNKNOWN, LOAD, PLAY, PAUSE, UNLOAD, ACQUIRE = range(6)
    def __init__(self, start, kind = Kind.UNKNOWN):
        self.kind = kind
        self.messages = [start]
        self.stages = []
        self.processes = { 'luna' : ProcessInstance('luna[0]') }
        self.expects = None
        self.session = start['msg']['info']['SESSION_ID']
        self.pipeline = None
    # is complete sequence?
    def complete(self):
        return not self.expects
    # append message
    def append(self, msg):
        self.messages.append(msg)
        pi = ProcessInstance(msg['syslog']['bsdtag'])
        if pi.name not in self.processes:
            self.processes[pi.name] = pi
        self.stages.append(Stage(self.messages[-2], self.messages[-1], self.processes))
    # try to handle message
    def handle(self, msg):
        return False
    # validate message
    def validate(self, msg):
        bsdtag = msg['syslog']['bsdtag']
        pname = Process.get_name(bsdtag)
        pid = ProcessInstance.get_pid(bsdtag)
        # validating process instances
        pi = self.processes.get(pname)
        if pi and pi.pid != pid:
            return False
        return True
    def pair_luna_message(self):
        ls_msg = self.messages[-1]['msg']['message']
        return LSMessage(LSMessage.Direction.INCOMMING, ls_msg.method, ls_msg.token)
    def handle_luna_message(self, msg):
        if self.expects:
            ls_msg = msg['msg']['message']
            if ((self.expects.direction == ls_msg.direction) and
                (self.expects.method == ls_msg.method) and
                #(not self.expects.token or self.expects.token == ls_msg.token) and
                (not self.expects.payload or self.expects.payload.keys()[0] in ls_msg.payload)):
                self.append(msg)
                if len(self.stages) == 1:
                    self.pipeline = msg['msg']['info']['SESSION_ID']
                return True
        return False
    def get_sequence_name(self):
        return {Sequence.Kind.LOAD   : 'LOAD SEQUENCE:   ',
                Sequence.Kind.PLAY   : 'PLAY SEQUENCE:   ',
                Sequence.Kind.PAUSE  : 'PAUSE SEQUENCE:  ',
                Sequence.Kind.UNLOAD : 'UNLOAD SEQUENCE: ',
                Sequence.Kind.ACQUIRE: 'ACQUIRE SEQUENCE: '}.get(self.kind)
    def print_header(self):
        csv = self.get_sequence_name() + '\n'
        csv += 'Session,' + str(self.session) + '\n'
        csv += 'Pipeline,' + str(self.pipeline) + '\n'
        return csv
    def print_stages(self):
        csv = ''
        for stage in self.stages:
            csv += str(stage.process) + ',' + stage.name + ',' + '%0.6f' % stage.elapsed + '\n'
        return csv
    def to_csv(self):
        csv = self.print_header()
        csv += self.print_stages()
        return csv
    def __repr__(self):
        view = '============ ' + self.get_sequence_name() + ' ============\n'
        view += 'Session: ' + str(self.session) + ', Pipeline: ' + str(self.pipeline) + '\n'
        for stage in self.stages:
            view += str(stage) + '\n'
        return view
    def __str__(self):
        return self.__repr__()
