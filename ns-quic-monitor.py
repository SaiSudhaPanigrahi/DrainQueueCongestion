#!/usr/bin/env python
import time
import os
import subprocess ,signal
prefix_cmd="./waf --run 'scratch/dqc-test --it=%s'"
test_case=9
for case in range(test_case):
    inst=str(case+1);
    cmd=prefix_cmd%(inst)
    process= subprocess.Popen(cmd,shell = True)
    while 1:
        time.sleep(1)
        ret=subprocess.Popen.poll(process)
        if ret is None:
            continue
        else:
            break  
print "stop"
