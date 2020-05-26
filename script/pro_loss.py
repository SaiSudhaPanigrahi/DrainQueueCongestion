'''
https://blog.csdn.net/qq_29422251/article/details/77713741
'''
import os
def mkdir(path):
    folder = os.path.exists(path)
    if not folder:    
        os.makedirs(path)
def ReadLossInfo(fileName):
    count=0
#cache the last line
    line=""
    for index, line in enumerate(open(fileName,'r')):
        count += 1
    lineArr = line.strip().split()
    return count,int(lineArr[1])
instance=1
flows=3;
algo="viva"
data_dir=algo+"/"
fileout="%s_loss_%s.txt"%(algo,str(flows))    
name="%s_%s_%s_owd.txt"
mkdir(data_dir)
fout=open(data_dir+fileout,'w')
for case in range(instance):
    total_recv=0
    total=0
    average_loss=0.0
    exist=False
    for i in range(flows):
        filename=name%(str(case+1),algo,str(i+1))
        if os.path.exists(filename):
            recv,max_recv=ReadLossInfo(filename)
            total_recv+=recv
            total+=max_recv
            exist=True
    if exist:
        average_loss=float(total-total_recv)/total
        fout.write(str(case+1)+"\t")
        fout.write(str(average_loss)+"\n")
    else:
        fout.write(str(case+1)+"\n")
fout.close()
