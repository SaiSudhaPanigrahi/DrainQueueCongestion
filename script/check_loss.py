filein="7_dqc_2_owd.txt"
last_seq=int(0)
count=0
fileout="loss.txt"
fout=open(fileout,'w') 
with open(filein) as txtData:
    for line in txtData.readlines():
        lineArr = line.strip().split()
        seq=int(lineArr[1])
        if(last_seq!=0):
            for i in range(last_seq+1,seq):
                fout.write(str(i)+"\n")
        last_seq=seq   
fout.close()        
