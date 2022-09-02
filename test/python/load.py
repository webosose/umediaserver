import sys, os
from uMediaServer.uMediaClient import MediaPlayer
from uMediaServer.uMSTest import uMSTest, cmd_line

class Test(uMSTest):
    suite = [('Load', 1000)]

    cnt = [0]
    last_idx = 0
    def progress(self, i):
        if self.last_idx != i:
            self.last_idx = i
            print
        self.cnt[i] += 1
        sys.stdout.write("." if self.cnt[i] % 10 else str(self.cnt[i]))

    def Load(self, video):
        self.progress(0)
	self.sendDebugMsg('exit_delay ' + str(conf.delay))

sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)

cl = cmd_line()
cl.add_argument('-c', '--count', type=int, default=1, help='how many times repeat test loop (default = 1)')
cl.add_argument('-d', '--delay', type=int, default=0, help='pipeline exit delay in milliseconds (default = 0)')
args = cl.get_args()
conf = cl.get_conf()

for i in xrange(conf.count):
    if conf.verbose: print('Test loop #%d' % (i+1))

    Test(**args).run()
