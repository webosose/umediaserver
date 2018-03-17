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

sys.path.insert(0, '../../src/')

import unittest
from detector import *

class TestDetector(unittest.TestCase):

    def setUp(self):
        self.fixture = {}
        self.fixture['server_play'] = {
            'syslog' : {
                'timestamp' : '2013-05-10T20:30:05.677987+03:00'
            },
            'msg' : {
                'context'   : '{ums.server}:',
                'message'   : 'play. cmd={"mediaId":"9tlXA1WJL1p6gWv"},connection_id=9tlXA1WJL1p6gWv'
            }
        }
        self.fixture['server_load'] = {
            'syslog' : {
                'timestamp' : '2013-05-10T20:29:53.654974+03:00'
            },
            'msg' : {
                'context'   : '{ums.server}:',
                'message'   : 'load. cmd={"uri":"file:///media/test/1.mov","type":"file","payload":""},connection_id=9tlXA1WJL1p6gWv'
            }
        }
        self.fixture['server_unload'] = {
            'syslog' : {
                'timestamp' : '2013-05-10T20:30:50.645600+03:00'
            },
            'msg' : {
                'context'   : '{ums.server}:',
                'message'   : 'unload. cmd={"mediaId":"9tlXA1WJL1p6gWv"},connection_id=9tlXA1WJL1p6gWv'
            }
        }
        self.fixture['server_pause'] = {
            'syslog' : {
                'timestamp' : '2013-05-10T20:30:09.894061+03:00'
            },
            'msg' : {
                'context'   : '{ums.server}:',
                'message'   : 'pause. cmd={"mediaId":"9tlXA1WJL1p6gWv"},connection_id=9tlXA1WJL1p6gWv'
            }
        }
        self.fixture['pipeline_play'] = {
            'syslog' : {
                'timestamp' : '2013-05-10T20:30:05.680934+03:00'
            },
            'msg' : {
                'context'   : '{ums.pipeline}:',
                'message'   : '     from PAUSED to PLAYING '
            }
        }
        self.fixture['pipeline_load'] = {
            'syslog' : {
                'timestamp' : '2013-05-10T20:29:53.929328+03:00'
            },
            'msg' : {
                'context'   : '{ums.pipeline}:',
                'message'   : '     from NULL to READY '
            }
        }
        self.fixture['pipeline_unload'] = {
            'syslog' : {
                'timestamp' : '2013-05-10T20:30:50.695053+03:00'
            },
            'msg' : {
                'context'   : '{ums.pipeline}:',
                'message'   : '*** set pipeline state to NULL successful'
            }
        }
        self.fixture['pipeline_pause'] = {
            'syslog' : {
                'timestamp' : '2013-05-10T20:30:09.894061+03:00'
            },
            'msg' : {
                'context'   : '{ums.pipeline}:',
                'message'   : '     from PLAYING to PAUSED '
            }
        }
        self.fixture['pipeline_start'] = {
            'syslog' : {
                'timestamp' : '2013-05-10T20:30:30.773189+03:00'
            },
            'msg' : {
                'context'   : '{ums.pipeline}:',
                'message'   : '########### Pipeline Process Started : PID: 27489 ###########'
            }
        }

    def test_server_events(self):
        event = Detector.detect(self.fixture['server_play'])
        self.assertEqual(event.source, EventSource.SERVER, 'should detect SERVER as a source')
        self.assertEqual(event.kind, EventKind.PLAY, 'should detect PLAY as a kind')
        event = Detector.detect(self.fixture['server_load'])
        self.assertEqual(event.source, EventSource.SERVER, 'should detect SERVER as a source')
        self.assertEqual(event.kind, EventKind.LOAD, 'should detect LOAD as a kind')
        event = Detector.detect(self.fixture['server_unload'])
        self.assertEqual(event.source, EventSource.SERVER, 'should detect SERVER as a source')
        self.assertEqual(event.kind, EventKind.UNLOAD, 'should detect UNLOAD as a kind')
        event = Detector.detect(self.fixture['server_pause'])
        self.assertEqual(event.source, EventSource.SERVER, 'should detect SERVER as a source')
        self.assertEqual(event.kind, EventKind.PAUSE, 'should detect PAUSE as a kind')


    def test_pipline_ctrl_events(self):
        self.assertTrue(None, 'todo: implement')

    def test_pipline_events(self):
        event = Detector.detect(self.fixture['pipeline_play'])
        self.assertEqual(event.source, EventSource.PIPELINE, 'should detect PIPELINE as a source')
        self.assertEqual(event.kind, EventKind.PLAY, 'should detect PLAY as a kind')
        event = Detector.detect(self.fixture['pipeline_load'])
        self.assertEqual(event.source, EventSource.PIPELINE, 'should detect PIPELINE as a source')
        self.assertEqual(event.kind, EventKind.LOAD, 'should detect LOAD as a kind')
        event = Detector.detect(self.fixture['pipeline_unload'])
        self.assertEqual(event.source, EventSource.PIPELINE, 'should detect PIPELINE as a source')
        self.assertEqual(event.kind, EventKind.UNLOAD, 'should detect UNLOAD as a kind')
        event = Detector.detect(self.fixture['pipeline_pause'])
        self.assertEqual(event.source, EventSource.PIPELINE, 'should detect PIPELINE as a source')
        self.assertEqual(event.kind, EventKind.PAUSE, 'should detect PAUSE as a kind')
        event = Detector.detect(self.fixture['pipeline_start'])
        self.assertEqual(event.source, EventSource.PIPELINE, 'should detect PIPELINE as a source')
        self.assertEqual(event.kind, EventKind.START, 'should detect START as a kind')


if __name__ == '__main__':
    unittest.main()
