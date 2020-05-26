#!/usr/bin/env python
import time
import os
import subprocess ,signal
prefix_cmd="./waf --run 'scratch/dqc-test --it=%s --cc=%s'"
test_cases=[1,2,3]
cc_name="bbr"
for case in range(len(test_cases)):
    inst=str(test_case[case]);
    cmd=prefix_cmd%(inst,cc_name)
    process= subprocess.Popen(cmd,shell = True)
    while 1:
        time.sleep(1)
        ret=subprocess.Popen.poll(process)
        if ret is None:
            continue
        else:
            break  
print "stop"
