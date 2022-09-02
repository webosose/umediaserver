import time
import threading
import Queue
import argparse
import sys

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

arg_parser = argparse.ArgumentParser(description='Performs repeated pipeline load performance test')
arg_parser.add_argument('files', nargs='+', action ='store', help ='media file to play')
arg_parser.add_argument('-i','--iterations',type=int, default=100,help='number of iterations')
arg_parser.add_argument('-t','--type',type=str,default='media',help='pipeline type')
arg_parser.add_argument('--tv', type=bool, default=False, help='enable tv-mode')
args = vars(arg_parser.parse_args())

try:
    recv = Queue.Queue()
    umc = MediaPlayer()
    start_proxy(umc, recv)

    for i in range(args['iterations']):
        sys.stdout.write('Iteration: ' + str(i) + '\n')
        for file in args['files']:
            try:
                umc.load('file://' + file, args['type'], '{}')
                wait_reply(recv, ['load'])
            finally:
                umc.unload()
                time.sleep(1)

except Exception as e:
    sys.stderr.write('Failed to initialize test: ' + e.args[0] + '\n')

# Instruction for running the performance measurements
#
# On Device:
#
# set the desired value for pool_size in /etc/umediaserver/umediaserver_resource_config.txt
# For this test, we ran 0,1,2
#
# Terminal 1
# python /usr/share/umediaserver/python/pipelinePoolPerf.py -i 100 -t media /usr/palm/applications/com.palm.app.firstuse/assets/interstitial/combinedVideos.mp4
#
# Terminal 2
# ls-monitor -f com.webos.media | grep load
#
# Post Processing of ls-monitor log on host machine
# moragues@moragues-mbp:~$ perl pipelinePoolLog.pl < mediapipeline0_build801.txt
# Iterations: 99
# Total time: 82.9699999999997
# Average time: 0.838080808080806
#
# moragues@moragues-mbp:~$ perl pipelinePoolLog.pl < mediapipeline1_build801.txt
# Iterations: 99
# Total time: 56.558
# Average time: 0.571292929292929
#
# moragues@moragues-mbp:~$ perl pipelinePoolLog.pl < mediapipeline2_build801.txt
# Iterations: 99
# Total time: 56.6239999999999
# Average time: 0.571959595959595
