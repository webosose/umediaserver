from random import random
from uMediaServer.uMSTest import uMSTest, cmd_line

class Test(uMSTest):
    suite = [('Client', 1)]

    def dice(self, sides = 6):
        return 1 + int(sides * random())

    def Client(self, video):
        pauses = dict([(random()*video.duration, self.dice(3)) for i in xrange(0, self.dice(2))])
        print("!!!", pauses)

        ct = 0
        self.do_one_cmd('play')

        for w in sorted(pauses, lambda a,b: cmp(a, b)):
            self.sleep(w - ct)
            self.do_one_cmd('pause')
            self.sleep(pauses[w]*1000)
            self.do_one_cmd('play')
            ct = w

        # if current time is second till end we most likely miss 'eos' event
        if ct < video.duration - 1000:
            print("!!! waiting for EOS")
            self.parse_reply(['eos'], timeout = 1)

        if self.dice() > 4:
            ct = random()*video.duration
            print("!!! seek back to", ct)
            self.do_one_cmd('seek', int(ct))
            self.do_one_cmd('play')
            self.sleep(video.duration - ct)
            print("!!! waiting for EOS")
            self.parse_reply(['eos'], timeout = 1)

Test(**(cmd_line().get_args())).run()
