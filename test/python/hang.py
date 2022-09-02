import threading
import Queue
import argparse

from uMediaServer.uMediaClient import MediaPlayer

def proxy_thr(recv, send):
    while True:
        (ev, data) = recv.get()
        print("ev '%s' = (%s)" % (ev, data))
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

arg_parser = argparse.ArgumentParser(description='Command line python tool. hang test')
arg_parser.add_argument('--tv', type=bool, default=False, help='enable tv-mode')
args = vars(arg_parser.parse_args())

recv = Queue.Queue()
umc = MediaPlayer()
start_proxy(umc, recv)

try:
    umc.load('file://dummy', "sim")
    wait_reply(recv, ['load'])
    umc.sendDebugMsg('hang')
    umc.play();  wait_reply(recv, ['play'])
finally:
    umc.unload()
