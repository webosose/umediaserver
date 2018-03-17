import Queue
import argparse
import sys

from uMediaServer.uMediaClient import MediaPlayer

def wait_reply(recv, tags, timeout = 5):
    while tags:
        try:
            (ev, data) = recv.get(timeout = timeout)
        except Queue.Empty: raise Exception('operation timeout')
        try:
            tags.remove(ev)
        except: pass

arg_parser = argparse.ArgumentParser(description='Performs load/unload stress test')
arg_parser.add_argument('files', nargs='+', action ='store', help ='media file to play')
arg_parser.add_argument('-t','--type',type=str,default='sim',help='pipeline type')
arg_parser.add_argument('--tv', type=bool, default=False, help='enable tv-mode')
args = vars(arg_parser.parse_args())

try:
    recv = Queue.Queue()
    umc = MediaPlayer()
    umc.setQueue(recv)

    for file in args['files']:
        umc.load('file://' + file, args['type'], '{}')
        wait_reply(recv, ['load'])
        umc.unload()

except Exception as e:
    sys.stderr.write('Failed to run test: ' + e.args[0] + '\n')
    exit(-1)
