import time
import Queue

from uMediaServer import uMediaClient

class Timer(object):
    def __init__(self, name="DUMMY"):
        self.name = name

    def __enter__(self):
        self.tstart = time.time()

    def __exit__(self, type, value, traceback):
        print "\nComplete '%s' in %.3fs" % (self.name, (time.time() - self.tstart))

class uMSTest:
    suite = {}

    def __init__(self, uri, umc=None, verbose=0, timeout=5, tv=False, media_class="file", payload=""):
        self.uri_ = uri
        self.class_ = media_class
        self.payload_ = payload
        self.timeout_ = timeout
        self.q_ = Queue.Queue()
        self.umc_ = umc if umc else uMediaClient.MediaPlayer()
        self.umc_.setQueue(self.q_)
        self.verbose_ = verbose
        if self.verbose_ > 2: print 'test: umc is', self.umc_

    def sendDebugMsg(self, msg):
        self.umc_.sendDebugMsg(msg)

    def sleep(self, ms):
        time.sleep(ms/1000.)

    def parse_reply(self, tags, timeout = None):
        class Resp: pass
        resp = Resp()

        if not timeout:
            timeout = self.timeout_

        if self.verbose_ > 2: print "going to wait for", tags
        while tags:
            try:
                (ev, data) = self.q_.get(timeout = timeout)
                if self.verbose_ > 2: print 'got ev "%s" (data = %s)' % (ev, data)
                if ev != 'unknown':
                    setattr(resp, ev, data)
                tags.remove(ev)
            except Queue.Empty: raise Exception('operation timeout')
            except Exception: pass
        return resp

    def do_one_cmd(self, cmd, *args):
        #mapping = {'seek': ('seek', 'play')}
        mapping = {}
        try:    wait = list(mapping[cmd])
        except: wait = [cmd]
        getattr(self.umc_, cmd)(*args)
        rep = self.parse_reply(list(wait))
        if not reduce(lambda a, v: a and getattr(rep, v), wait, 1):
            raise Exception(cmd + ' error')

    def run(self):
        for (tn, tc) in self.suite:
            tf = getattr(self, tn)
            if not tf: raise Exception('test "%s" specified in suite cannot be found' % tn)
            with Timer(tn):
                for i in xrange(tc):
                    if self.verbose_: print 'test case "%s", loop #%d' % (tn, i+1)
                    self.umc_.loadAsync(self.uri_, self.class_, self.payload_)
                    video = self.parse_reply(['load', 'duration'])
                    if self.verbose_ > 1: print 'video (duration = {0:.3f}s) loaded'.format(video.duration/1000.)

                    try:
                        tf(video)
                    except Exception as e:
                        print "Unexpected error:", e.args[0]
                    finally:
                        self.umc_.unload()
