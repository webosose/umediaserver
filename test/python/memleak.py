import subprocess, time, re
from uMediaServer.uMediaClient import MediaPlayer
from uMediaServer.uMSTest import uMSTest, cmd_line

class Test(uMSTest):
    suite = [('OneShot', 1)]

    def OneShot(self, video):
        self.do_one_cmd('play')
        self.sleep(5000)

        self.do_one_cmd('seek', 0)

        self.do_one_cmd('play')
        self.sleep(5000)

def get_mem_info(pid):
    stat = {}
    r = re.compile(r'^Vm(\w+):\s+(\d+) kB$')

    f = open("/proc/"+str(pid)+"/status");
    try:
        for line in f:
            m = r.match(line)
            if m:
                stat[m.group(1)] = int(m.group(2))
    finally:
        f.close()

    return stat

def print_mem_info_diff(m1, m2):
    for k in m2:
        if m2[k] > m1[k]:
            print "usage of", k, "increased from", m1[k], "kB to", m2[k], "kB"

def get_duration(raw):
    scale = { 's': 1, 'm': 60, 'h': 60*60, 'd': 24*60*60 }
    duration = 0
    for (d, s) in re.findall(r'^(\d+)([dhms])', raw):
        duration += int(d) * scale[s]
    return duration

cl = cmd_line()
cl.add_argument('-d', '--duration', default="1h", help='how much time spent running test loop (default = 1h)')
conf = cl.get_conf()

pid = int(subprocess.check_output(["ps", "h", "-opid", "-C", "umediaserver"]))
duration = get_duration(conf.duration)
test = Test(**(cl.get_args()))

# if ums fresh started it use very low memory and not fully initialized, so
# make it fat right now
test.run()

# now gather memory info
meminfo_start = get_mem_info(pid)
meminfo_last  = meminfo_start

if conf.verbose:
    print "initial mem info:", meminfo_start

begin = time.time()
while time.time() - begin < duration:
    test.run()
    meminfo = get_mem_info(pid)
    if conf.verbose: print "current mem info:", meminfo
    print_mem_info_diff(meminfo_last, meminfo)
    meminfo_last = meminfo

print "--- overal change info ---"
print_mem_info_diff(meminfo_start, meminfo_last)
