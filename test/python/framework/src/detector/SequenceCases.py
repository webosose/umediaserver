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

import re, json
from Sequence import Sequence, LSMessage, Stage

class LoadSequence(Sequence):
    METHOD = 'load'
    __pipeline_start = re.compile(r'Started .+ pipeline_process pid \d+')
    def __init__(self, msg):
        super(LoadSequence, self).__init__(msg, Sequence.Kind.LOAD)
        self.expects = self.pair_luna_message()
    def complete(self):
        return len(self.stages) == 10
    def handle(self, msg):
        if not self.validate(msg):
            return False
        # expecting pipeline process start
        if (len(self.stages) == 1 and type(msg['msg']['message']) == str and
            LoadSequence.__pipeline_start.match(msg['msg']['message'])):
            self.append(msg)
            self.stages[-1].name = 'forking pipeline'
            return True
        ls_msg = LSMessage.parse_message(msg['msg']['message'])
        # process luna_message
        if ls_msg:
            msg['msg']['message'] = ls_msg
            if self.handle_luna_message(msg):
                self.expects = { 1 : LSMessage(LSMessage.Direction.OUTGOING, 'ready'),
                                 3 : self.pair_luna_message(),
                                 4 : LSMessage(LSMessage.Direction.OUTGOING, 'load'),
                                 5 : self.pair_luna_message(),
                                 6 : LSMessage(LSMessage.Direction.OUTGOING, 'notify',
                                               None, {'loadCompleted':''}),
                                 7 : LSMessage(LSMessage.Direction.INCOMMING, 'notify',
                                               None, {'loadCompleted':''}),
                                 8 : LSMessage(LSMessage.Direction.OUTGOING, 'notify',
                                               None, {'loadCompleted':''}),
                                 9 : LSMessage(LSMessage.Direction.INCOMMING, 'notify',
                                               None, {'loadCompleted':''})
                               }.get(len(self.stages))
                sname = { 3 : 'starting pipeline',
                          5 : 'sending load',
                          7 : 'loading media',
                          9 : 'sending completion'}.get(len(self.stages))
                if sname:
                    self.stages[-1].name = sname
                return True
        return False

class PlaySequence(Sequence):
    METHOD = 'play'
    def __init__(self, msg):
        super(PlaySequence, self).__init__(msg, Sequence.Kind.PLAY)
        self.expects = self.pair_luna_message()
    def handle(self, msg):
        if not self.validate(msg):
            return False
        ls_msg = LSMessage.parse_message(msg['msg']['message'])
        # process luna_message
        if ls_msg:
            msg['msg']['message'] = ls_msg
            if self.handle_luna_message(msg):
                self.expects = { 1 : LSMessage(LSMessage.Direction.OUTGOING, 'play',
                                               None, {'data':''}),
                                 2 : self.pair_luna_message(),
                                 3 : LSMessage(LSMessage.Direction.OUTGOING, 'notify',
                                               None, {'playing':''}),
                                 4 : LSMessage(LSMessage.Direction.INCOMMING, 'notify',
                                               None, {'playing':''}),
                                 5 : LSMessage(LSMessage.Direction.OUTGOING, 'notify',
                                               None, {'playing':''}),
                                 6 : LSMessage(LSMessage.Direction.INCOMMING, 'notify',
                                               None, {'playing':''})
                               }.get(len(self.stages))
                sname = { 2 : 'sending play',
                          4 : 'playing media',
                          6 : 'sending completion'}.get(len(self.stages))
                if sname:
                    self.stages[-1].name = sname
                return True
        return False

class PauseSequence(Sequence):
    METHOD = 'pause'
    def __init__(self, msg):
        super(PauseSequence, self).__init__(msg, Sequence.Kind.PAUSE)
        self.expects = self.pair_luna_message()
    def handle(self, msg):
        if not self.validate(msg):
            return False
        ls_msg = LSMessage.parse_message(msg['msg']['message'])
        # process luna_message
        if ls_msg:
            msg['msg']['message'] = ls_msg
            if self.handle_luna_message(msg):
                self.expects = { 1 : LSMessage(LSMessage.Direction.OUTGOING, 'pause',
                                               None, {'data':''}),
                                 2 : self.pair_luna_message(),
                                 3 : LSMessage(LSMessage.Direction.OUTGOING, 'notify',
                                               None, {'paused':''}),
                                 4 : LSMessage(LSMessage.Direction.INCOMMING, 'notify',
                                               None, {'paused':''}),
                                 5 : LSMessage(LSMessage.Direction.OUTGOING, 'notify',
                                               None, {'paused':''}),
                                 6 : LSMessage(LSMessage.Direction.INCOMMING, 'notify',
                                               None, {'paused':''})
                               }.get(len(self.stages))
                sname = { 2 : 'sending pause',
                          4 : 'pausing media',
                          6 : 'sending completion'}.get(len(self.stages))
                if sname:
                    self.stages[-1].name = sname
                return True
        return False

class UnloadSequence(Sequence):
    METHOD = 'unload'
    __pipeline_death = re.compile(r'Handling SIGCHLD from pid: \d+')
    def __init__(self, msg):
        super(UnloadSequence, self).__init__(msg, Sequence.Kind.UNLOAD)
        self.expects = self.pair_luna_message()
    def handle(self, msg):
        if not self.validate(msg):
            return False
        if (len(self.stages) == 3 and type(msg['msg']['message']) == str and
            UnloadSequence.__pipeline_death.match(msg['msg']['message'])):
            self.append(msg)
            self.stages[-1].name = 'exiting pipeline'
            self.expects = None
            return True
        ls_msg = LSMessage.parse_message(msg['msg']['message'])
        # process luna_message
        if ls_msg:
            msg['msg']['message'] = ls_msg
            if self.handle_luna_message(msg):
                self.expects = { 1 : LSMessage(LSMessage.Direction.OUTGOING, 'unload',
                                               None, {'data':''}),
                                 2 : self.pair_luna_message()
                               }.get(len(self.stages))
                sname = { 2 : 'sending unload',
                          4 : 'unloading media',
                          6 : 'sending completion'}.get(len(self.stages))
                if sname:
                    self.stages[-1].name = sname
                return True
        return False

class AcquireSequence(Sequence):
    METHOD = 'acquire'
    _acquire_conflict = re.compile(r'ACQUIRE failed\. Invoking Policy')
    def __init__(self, msg):
        super(AcquireSequence, self).__init__(msg, Sequence.Kind.ACQUIRE)
        self.expects = self.pair_luna_message()
        self.hasConflict = False
    def handle(self, msg):
        if not self.validate(msg):
            return False

        if (len(self.stages) == 1 and type(msg['msg']['message']) == str and
            AcquireSequence._acquire_conflict.match(msg['msg']['message'])):
            self.expects = LSMessage(LSMessage.Direction.OUTGOING, 'policyAction', None, {'policyAction':''})
            self.hasConflict = True
            return True

        ls_msg = LSMessage.parse_message(msg['msg']['message'])
        if (self.hasConflict and ls_msg):
             msg['msg']['message'] = ls_msg
             if self.handle_luna_message(msg):
                self.expects = {
                                 2 : LSMessage(LSMessage.Direction.INCOMMING, 'policyAction',
                                               None, {'policyAction':''}),
                                 3 : LSMessage(LSMessage.Direction.OUTGOING, 'policyAction',
                                               None, {'returnValue':''}),
                                 4 : LSMessage(LSMessage.Direction.OUTGOING, 'acquireComplete',
                                               None, {'resources':''}),
                                 5 : LSMessage(LSMessage.Direction.INCOMMING, 'acquireComplete',
                                               None, {'resources':''})
                }.get(len(self.stages))
                sname = {
                          2 : 'acquire conflict',
                          3 : 'policy action',
                          4 : 'resource release',
                          5 : 'acquisition'
                }.get(len(self.stages))
                if sname:
                    self.stages[-1].name = sname
                return True

        if ls_msg:
            msg['msg']['message'] = ls_msg

            if self.handle_luna_message(msg):
                self.expects = {
                                1 : LSMessage(LSMessage.Direction.OUTGOING, 'acquireCgit omplete',
                                               None, {'resources':''}),
                                2 : LSMessage(LSMessage.Direction.INCOMMING, 'acquireComplete',
                                               None, {'resources':''})
                        }.get(len(self.stages))
                sname = { 2 : 'acquisition' }.get(len(self.stages))
                if sname:
                    self.stages[-1].name = sname
                if self.complete():
                    self.stages.insert(1, Stage(None, None, None, 'acquire conflict'))
                    self.stages.insert(2, Stage(None, None, None, 'policy action'))
                    self.stages.insert(3, Stage(None, None, None, 'resource release'))
                return True

        return False

    def print_header(self):
        csv = super(AcquireSequence, self).print_header()
        self.resources = self.messages[0]['msg']['message'].payload['resources']
        self.resources = json.loads(self.resources)
        for resource in self.resources:
            csv += 'Resource,' + str(resource['resource']) + '\n'
            csv += 'Quantity,' + str(resource['qty']) + '\n'
            csv += 'Index,' + str(resource['index']) + '\n'
            return csv