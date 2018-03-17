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

class Format:
    RAW, CSV, JSON = range(3)

class Formatter:
    @staticmethod
    def format(log_message, format = Format.RAW):
        def format_raw(log_message):
            return log_message['raw'].rstrip()

        def format_csv(log_mesage):
            s = log_message['syslog']
            m = log_message['msg']
            csv = ','.join([str(s['timestamp']), str(s['uptime']), str(s['hostname']), str(s['priority']),
                            str(s['bsdtag']), str(m['context']), str(m['timestamp']), str(m['session']),
                            str(m['codepoint']), str(m['message'])])
            return csv

        def format_json(log_message):
            return str({'syslog' : log_message['syslog'], 'msg' : log_message['msg']})

        if format == Format.RAW:
            return format_raw(log_message)
        elif format == Format.CSV:
            return format_csv(log_message)
        elif format == Format.JSON:
            return format_json(log_message)

        raise ValueError('Unknown format')
