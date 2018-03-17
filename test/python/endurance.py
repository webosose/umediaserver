import sys, os
from uMediaServer.uMediaClient import MediaPlayer
from uMediaServer.uMSTest import uMSTest, cmd_line

class Test(uMSTest):
    suite = [('Load', 1000), ('TrickPlay', 1), ('Restart', 1)]

    cnt = [0, 0, 0]
    last_idx = 0
    def progress(self, i):
        if self.last_idx != i:
            self.last_idx = i
            print
        self.cnt[i] += 1
        sys.stdout.write("." if self.cnt[i] % 10 else str(self.cnt[i]))

    def Load(self, video):
        self.progress(0)
        self.sleep(100)

    def TrickPlay(self, video):
        for _xxx in xrange(500):
            self.progress(1)
            self.do_one_cmd('play')
            self.sleep(5000)

            self.do_one_cmd('seek', 0)

            self.do_one_cmd('play')
            self.sleep(5000)

    def Restart(self, video):
        for _xxx in xrange(500):
            self.progress(2)
            self.do_one_cmd('play')
            self.sleep(video.duration)
            try: self.parse_reply(['eos'], timeout = 1)
            except: raise Exception('playback failure')

            self.do_one_cmd('seek', 0)

sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)

cl = cmd_line()
cl.add_argument('-c', '--count', type=int, default=1, help='how many times repeat test loop (default = 1)')
cl.add_argument('-n', '--noreuse', type=bool, default=False, help='do not use new umc object for each test loop (default = False)')
args = cl.get_args()
conf = cl.get_conf()

global umc
if not conf.noreuse:
    umc = MediaPlayer()
else:
    umc = None

for i in xrange(conf.count):
    if conf.verbose: print 'Test loop #%d' % (i+1)
    if conf.verbose > 2: print 'umc is', umc

    Test(umc=umc, **args).run()
