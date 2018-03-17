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

import re
import json
import string
from SequenceCases import *

class SequenceFactory:
    @staticmethod
    def try_create(msg):
        try:
            ls_msg = LSMessage.parse_message(msg['msg']['message'])
            if ls_msg:
                msg['msg']['message'] = ls_msg
                if ls_msg.direction == LSMessage.Direction.OUTGOING:
                    def check_create_load():
                        if 'uri' in ls_msg.payload:
                            return LoadSequence(msg)
                        return None
                    def check_create_play():
                        if ls_msg.payload.keys() == ['mediaId']:
                            return PlaySequence(msg)
                        return None
                    def check_create_pause():
                        if ls_msg.payload.keys() == ['mediaId']:
                            return PauseSequence(msg)
                        return None
                    def check_create_unload():
                        if ls_msg.payload.keys() == ['mediaId']:
                            return UnloadSequence(msg)
                        return None
                    def check_create_acquire():
                        if 'resources' in ls_msg.payload:
                            return AcquireSequence(msg)
                    return { LoadSequence.METHOD   : check_create_load(),
                             PlaySequence.METHOD   : check_create_play(),
                             PauseSequence.METHOD  : check_create_pause(),
                             UnloadSequence.METHOD : check_create_unload(),
                             AcquireSequence.METHOD: check_create_acquire()
                    }.get(ls_msg.method)
            return None
        except Exception as e:
            return None

class LogBuffer:
    def __init__(self):
        self.buffer = []
    def push(self, log):
        idx = len(self.buffer)
        for i in xrange(len(self.buffer) - 1, -1, -1):
            if self.buffer[i]['msg']['info']['TIMESTAMP'] < log['msg']['info']['TIMESTAMP']:
                idx = i + 1
                break
        self.buffer.insert(idx, log)
    def pop(self):
        return self.buffer.pop(0)
    def len(self):
        return len(self.buffer)

class Detector:
    __buffer_depth = 20
    def __init__(self):
        self.sequence_kinds = dict()
        self.buffer = LogBuffer()
    def __process(self, message):
                # first try to feed message to existing sequences
        for kind, sequences in self.sequence_kinds.iteritems():
            for sequence in sequences:
                if sequence.handle(message):
                    return True
        # try to create new sequence
        sequence = SequenceFactory.try_create(message)
        if sequence:
            if sequence.kind not in self.sequence_kinds:
                self.sequence_kinds[sequence.kind] = [sequence]
                return True
            self.sequence_kinds[sequence.kind].insert(0,sequence)
            return True
        # message skipped
        return False
    def process(self, message):
        self.buffer.push(message)
        if (self.buffer.len() < Detector.__buffer_depth):
            return True
        return self.__process(self.buffer.pop())
    def finalize(self):
        # process rest of the buffer
        while (self.buffer.len()):
            self.__process(self.buffer.pop())
        sequence_kinds = dict()
        for kind in self.sequence_kinds:
            sequences = []
            for sequence in self.sequence_kinds[kind]:
                if sequence.complete():
                    sequences.append(sequence)
            sequence_kinds[kind] = sequences
        self.sequence_kinds = sequence_kinds
    def summary(self):
        csv = open('./report.csv', 'w')
        summary = ''
        for kind, sequences in self.sequence_kinds.iteritems():
            stages = []
            for sequence in sequences:
                csv.write(sequence.to_csv())

                if len(stages):
                    for idx in range(len(sequence.stages)):
                        stage = sequence.stages[idx]
                        stages[idx]['elapsed'] += stage.elapsed
                else:
                    for idx in range(len(sequence.stages)):
                        stage = sequence.stages[idx]
                        procname = stage.process.name
                        if 'server' in procname:
                            procname = 'server'
                        elif not 'luna' == procname:
                            procname = 'pipeline'
                        stages.append({'process' : procname, 'descr' : stage.name,
                                       'elapsed' : stage.elapsed})
            summary += '============ ' + {Sequence.Kind.LOAD   : ' LOAD:   (',
                                          Sequence.Kind.PLAY   : ' PLAY:   (',
                                          Sequence.Kind.PAUSE  : ' PAUSE:  (',
                                          Sequence.Kind.UNLOAD : ' UNLOAD: (',
                                          Sequence.Kind.ACQUIRE: 'ACQUIRE: ('
                                         }.get(kind) + str(len(sequences)) + ') ============\n'
            for stage in stages:
                summary += string.ljust(stage['process'] + ':', 10)
                summary += string.ljust(stage['descr'] + ':', 20)
                summary += '%0.6f\n' % (stage['elapsed'] / len(sequences))

        return summary.rstrip()
