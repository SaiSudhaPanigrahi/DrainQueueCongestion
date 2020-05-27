#!/usr/bin/env python
import os
def mkdir(path):
    folder = os.path.exists(path)
    if not folder:    
        os.makedirs(path)
def ReafByteInfo(fileName):
    bytes=0
    for index, line in enumerate(open(fileName,'r')):
        lineArr = line.strip().split()
        bytes=bytes+int(lineArr[3])
    return bytes
def CoutByteForFlows(ins,algo,flows):
    name=str(ins)+"_"+algo+"_"+"%s"+"_owd.txt"
    bytes=0;
    for i in range(flows):
        filename=name%str(i+1)
        if os.path.exists(filename):
            bytes=bytes+ReafByteInfo(filename)
        else:
            bytes=0
            break
    return bytes
name="%s_util.txt"
instance=1
flows=5
algo="bbr"
data_dir=algo+"/"
mkdir(data_dir)
fileout=name%algo
caps=[5000000,3000000,3000000]
duration=400;
fout=open(data_dir+fileout,'w')
for case in range(instance):
    bytes=0
    total=caps[case]*duration/8
    bytes=CoutByteForFlows(case+1,algo,flows)
    util=float(bytes)/float(total)
    fout.write(str(case+1)+"\t")
    fout.write(str(util)+"\n")
fout.close()
