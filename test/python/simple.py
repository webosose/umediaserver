import time
import threading
import Queue
import argparse
import sys

from uMediaServer.uMediaClient import MediaPlayer

def proxy_thr(recv, send):
    while True:
        (ev, data) = recv.get()
        print "ev '%s' = (%s)" % (ev, data)
        send.put_nowait((ev, data))

def start_proxy(umc, send):
    recv = Queue.Queue()
    umc.setQueue(recv)

    ev_handler = threading.Thread(target=proxy_thr, args=(recv,send))
    ev_handler.daemon = True
    ev_handler.start()

def wait_reply(recv, tags, timeout = 5):
    while tags:
        try:
            (ev, data) = recv.get(timeout = timeout)
        except Queue.Empty: raise Exception('operation timeout')
        try:
            tags.remove(ev)
        except: pass

arg_parser = argparse.ArgumentParser(description='Performs simple playback test')
arg_parser.add_argument('files', nargs='+', action ='store', help ='media file to play')
arg_parser.add_argument('-t','--type',type=str,default='sim',help='pipeline type')
arg_parser.add_argument('--tv', type=bool, default=False, help='enable tv-mode')
args = vars(arg_parser.parse_args())

try:
    recv = Queue.Queue()
    umc = MediaPlayer()
    start_proxy(umc, recv)

    for file in args['files']:
        try:
            umc.load('file://' + file, args['type'], '{}')
            wait_reply(recv, ['load'])
            umc.play();  wait_reply(recv, ['play'])
            time.sleep(5)
            umc.pause(); wait_reply(recv, ['pause'])
            time.sleep(5)
            umc.play();  wait_reply(recv, ['play'])
            time.sleep(5)
            umc.seek(0);
            umc.play();  wait_reply(recv, ['play'])
            wait_reply(recv, ['eos'])
        finally:
            umc.unload()
            time.sleep(1)

except Exception as e:
    sys.stderr.write('Failed to initialize test: ' + e.args[0] + '\n')
