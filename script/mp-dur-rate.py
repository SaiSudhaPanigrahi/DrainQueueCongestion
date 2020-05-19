import os
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

algo="lia"
#algo="bbrrand"
fileName1="%s_"+algo+"_1_owd.txt"
fileName2="%s_"+algo+"_2_owd.txt"
fileOutName="mp_"+algo+"_bw"
duration=300.0
gap=5.0
total=int(duration/gap);
instance=5
for case in range(instance):
    fileOut=fileOutName+"_"+str(case+1)+".txt"
    fout=open(fileOut,'w')
    for i in range(total):
        left=i*gap
        right=(i+1)*gap
        f1=fileName1%(str(case+1))
        f2=fileName2%(str(case+1))
        bytes1=ReafByteInfo(f1,left,right)
        bytes2=ReafByteInfo(f2,left,right)
        rate=(bytes1+bytes2)*8/(gap*1000)
        fout.write(str(right)+"\t"+str(rate)+"\n")
    fout.close()
