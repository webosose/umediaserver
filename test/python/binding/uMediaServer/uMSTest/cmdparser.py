import argparse
from uMediaServer import uMediaClient

def map_pass(v):
    return v

class def_map_t(dict):
    def __missing__(self, key): return map_pass

name_map = def_map_t({
    'class': lambda _: 'media_class'
    })
value_map = def_map_t({
    })

class cmd_line:
    def __init__(self):
        parser = argparse.ArgumentParser()
        parser.add_argument('-t', '--timeout', type=int, default=5, help='how long wait for answer, in seconds (default = 5)')
        parser.add_argument('-v', '--verbose', default=0, action='count', help='verbosity level (default = 0)')
        parser.add_argument('--tv', type=bool, default=False, help='enable tv-mode')
        parser.add_argument('uri', help='uri to play')
        parser.add_argument('class', default="file", nargs='?', help='media class')
        parser.add_argument('payload', default="", nargs='?', help='payload for media class')
        self.parser = parser
        self.conf = None

    def add_argument(self, *args, **kwargs):
        self.parser.add_argument(*args, **kwargs)

    def get_conf(self):
        if not self.conf:
            self.conf = self.parser.parse_args()
        return self.conf

    def get_args(self, _all = False):
        conf = self.get_conf()
        c1 = lambda x: x[0:1] != "_"
        c2 = lambda x: x in ['uri', 'class', 'payload', 'verbose', 'timeout', 'tv'] and hasattr(conf, x)
        check = c1 if _all else c2

        args = {}
        for arg in filter(check, dir(conf)):
            args[name_map[arg](arg)] = value_map[arg](getattr(conf, arg))

        return args
