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

class LogParseError(Exception):
    pass

class Parser:
    __patterns = {
        'rfc3164'     : re.compile(r'[A-Z][a-z][a-z]\s+\d{1,2}\s+\d{2}:\d{2}:\d{2}'),
        'rfc3339'     : re.compile(r'\d{4}-\d{2}-\d{2}[Tt]\d{2}:\d{2}:\d{2}\.\d{3,6}?(?:[+-]\d{2}:\d{2}|[Zz])'),
        'uptime'      : re.compile(r'\[\d+\]'),
        'priority'    : re.compile(r'\S+'),
        'bsdtag'      : re.compile(r'\w+(\[\d+(:\d+)?\])?'),
        'pmlog'       : re.compile(r'\[pmlog\]'),
        'context'     : re.compile(r'ums\.\w+'),
        'br'          : re.compile(r'\{\}'),
        'error'       : re.compile(r'[A-Z_]+'),
        'json'        : re.compile(r'\{".+?\}\s'),
        'timestamp'   : re.compile(r'\d+\.\d{9}'),
        'session'     : re.compile(r'([.a-z]+_)?[0-9,a-g,i-z,A-G,I-Z]{9,15}'),
        'junk'        : re.compile(r'[^<\s]+'),
        'codepoint'   : re.compile(r'<[^>]+>'),
        'whitespaces' : re.compile(r'\s*')
    }
    # TODO: remove this constant and its handling when log will output unified session ids
    __session_id_len = 15

    @staticmethod
    def parse(line):
        pos = 0
        parsed = {'syslog' : None, 'msg' : None, 'raw' : line}

        def match_string(string, pattern):
            m = pattern.match(string)
            return m.group(0) if m else None

        def count_whitespaces(string):
            return len(Parser.__patterns['whitespaces'].match(string).group(0))

        def parse_syslog_header(line):
            pos = 0
            syslog_header = {}

            def parse_timestamp(line):
                timestamp = match_string(line, Parser.__patterns['rfc3339'])
                if not timestamp:
                    timestamp = match_string(line, Parser.__patterns['rfc3164'])
                if not timestamp:
                    raise LogParseError('Failed to parse timestamp')
                l = len(timestamp)
                return timestamp, l

            def parse_uptime(line):
                uptime = match_string(line, Parser.__patterns['uptime'])
                return uptime, (len(uptime) if uptime else 0)


            def parse_priority(line):
                priority = match_string(line, Parser.__patterns['priority'])
                return priority, (len(priority) if priority else 0)

            def parse_bsdtag(line):
                bsdtag = match_string(line, Parser.__patterns['bsdtag'])
                return bsdtag, (len(bsdtag) if bsdtag else 0)


            syslog_header['timestamp'], l = parse_timestamp(line[pos:])
            pos += l; pos += count_whitespaces(line[pos:])
            syslog_header['uptime'], l = parse_uptime(line[pos:])
            pos += l; pos += count_whitespaces(line[pos:])
            syslog_header['priority'], l = parse_priority(line[pos:])
            pos += l; pos += count_whitespaces(line[pos:])
            syslog_header['bsdtag'], l = parse_bsdtag(line[pos:])
            pos += l; pos += count_whitespaces(line[pos:])

            return syslog_header, pos

        def parse_log_message(line):
            pos = 0
            log_message = {}

            def parse_pmlog(line):
                pmlog = match_string(line, Parser.__patterns['pmlog'])
                return pmlog, (len(pmlog) if pmlog else 0)

            def parse_br_or_error(line):
                brerror = match_string(line, Parser.__patterns['br'])
                if not brerror:
                    brerror = match_string(line, Parser.__patterns['error'])
                return brerror, (len(brerror) if brerror else 0)


            def parse_context(line):
                context = match_string(line, Parser.__patterns['context'])
                if not context:
                    raise LogParseError('Failed to parse context')
                return context, (len(context) if context else 0)

            def parse_json(line):
                jsonobj = match_string(line, Parser.__patterns['json'])
                jsonobj = jsonobj[:-1]
                return json.loads(jsonobj), (len(jsonobj) if jsonobj else 0)

            def parse_message(line):
                return line.rstrip(), (len(line) if line else 0)

            null, l = parse_pmlog(line[pos:])
            pos += l; pos += count_whitespaces(line[pos:])
            log_message['context'], l = parse_context(line[pos:])
            pos += l; pos += count_whitespaces(line[pos:])
            null, l = parse_br_or_error(line[pos:])
            pos += l; pos += count_whitespaces(line[pos:])
            log_message['info'], l = parse_json(line[pos:])
            pos += l; pos += count_whitespaces(line[pos:])
            log_message['message'], l = parse_message(line[pos:])
            pos += l; pos += count_whitespaces(line[pos:])




            return log_message, pos

        try:
            parsed['syslog'], pos = parse_syslog_header(line[pos:])
            parsed['msg'], pos = parse_log_message(line[pos:])
        except LogParseError as e:
            raise LogParseError(e.args[0] + ': ' + line)

        return parsed
