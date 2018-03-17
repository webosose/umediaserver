import sys, os, time
import Queue
import threading

from uMediaServer.uMediaClient import MediaPlayer
from uMediaServer.uMSTest import cmd_line

def proxy_thr(num, recv, send):
    while True:
        (ev, data) = recv.get()
        print '!!! %02d -> %s = %s' % (num, ev, data)
        send.put_nowait((ev, data))

def start_proxy(num, umc, send):
    recv = Queue.Queue()
    umc.setQueue(recv)

    ev_handler = threading.Thread(target=proxy_thr, args=(num,recv,send))
    ev_handler.daemon = True
    ev_handler.start()

def play(uri, wait=1, number=3, count=1, verbose=0, timeout=5, media_class="sim", payload="", flush=True, tv=False):
    def flush_queue(queue):
        while not queue.empty():
            (ev, data) = queue.get_nowait()
            if verbose > 0: print '<<< %s = %s' % (ev, data)
    def parse_reply(queue, tags):
        class Resp: pass
        resp = Resp()

        if verbose > 0: print ">>> wait for", tags
        while tags:
            try:
                (ev, data) = queue.get(timeout = timeout)
                if verbose > 0: print '>>> %s = %s' % (ev, data)
                if ev != 'unknown':
                    setattr(resp, ev, data)
                tags.remove(ev)
            except Queue.Empty: raise Exception('operation timeout')
            except Exception: pass
        return resp

    if verbose == 0:
        sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)

    print "Working", ("with" if flush else "without"), "flush"
    queue = []
    umc = []
    for i in xrange(number):
        if verbose > 0:
            print "load(%d)" % i
        else:
            sys.stdout.write("loading %d\n" % (i+1))
        queue.append(Queue.Queue())
        umc.append(MediaPlayer())
        if verbose > 1:
            start_proxy(i, umc[i], queue[i])
        else:
            umc[i].setQueue(queue[i])
        umc[i].load(uri, media_class, payload); parse_reply(queue[i], ['load', 'duration'])
        umc[i].play();                          parse_reply(queue[i], ['play'])
        time.sleep(wait)

    for k in xrange(count):
        for i in xrange(number):
            if verbose > 0:
                print "play(%d)" % i
            else:
                sys.stdout.write("->%d" % (i+1))
            if flush:
                flush_queue(queue[i])
            umc[i].play()
            r = parse_reply(queue[i], ['play'])
            if hasattr(r, 'eos'):
                umc[i].seek(0)
            time.sleep(wait)
        if verbose == 0:
            sys.stdout.write("\n")

cl = cmd_line()
cl.add_argument('-c', '--count', type=int, default=10, help='how many times do full round switching (default = 10)')
cl.add_argument('-n', '--number', type=int, default=3, help='how many clients will fight for resources (default = 3)')
cl.add_argument('-w', '--wait', type=float, default=0.1, help='how long wait before client unsuspending (default = 0.1)')
cl.add_argument('-f', '--flush', type=int, default=1, help='do queue flush before event (default = True)')

play(**(cl.get_args(True)))
