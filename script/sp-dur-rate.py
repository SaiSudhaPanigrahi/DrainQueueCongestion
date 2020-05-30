import os
def mkdir(path):
    folder = os.path.exists(path)
    if not folder:    
        os.makedirs(path)
def ReafByteInfo(fileName,left,right):
    bytes=0
    for index, line in enumerate(open(fileName,'r')):
        lineArr = line.strip().split()
        time=float(lineArr[0])
        if time>right:
            break
        if time>=left:
            bytes=bytes+int(lineArr[3])
    return bytes
algo="reno"
data_dir=algo+"/"
flows=4
fileName="%s_"+algo+"_%s_owd.txt"
fileOutName="%s_"+algo+"_%s_dur_bw"
duration=400.0
gap=5.0
total=int(duration/gap);
instance=[4]
data_dir=algo+"/"
mkdir(data_dir)
for case in range(len(instance)):
    for f in range(flows):
        strTemp=fileOutName%(str(instance[case]),str(f+1))
        fileOut=strTemp+".txt"
        fout=open(data_dir+fileOut,'w')
        for i in range(total):
            left=i*gap
            right=(i+1)*gap
            f1=fileName%(str(instance[case]),str(f+1))
            bytes1=ReafByteInfo(f1,left,right)
            rate=(bytes1)*8/(gap*1000)
            if rate>0.1:
                fout.write(str(right)+"\t"+str(rate)+"\n")
        fout.close()
