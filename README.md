# DrainQueueCongestion
Always drain buffer occupation in routers.  
Copy code from quic protocol for simualtion purpose.  
TODO  
- [x] Stream  
- [x] Ack  
- [x] Stop waiting  
- [x] Send packet through socket  
- [x] congestion control   

As for ns3 test case, the wscript file gives clear hint how to arange this file  
in the right position in ns3.  
And add the CPLUS_INCLUDE_PATH flag in /etc/profile, for example:  
```
export CPLUS_INCLUDE_PATH=CPLUS_INCLUDE_PATH:/home/zsy/C_Test/ns-allinone-3.26/ns-3.26/src/dqc/model/thirdparty/include:/home/zsy/C_Test/ns-allinone-3.26/ns-3.26/src/dqc/model/thirdparty/congestion/:/home/zsy/C_Test/ns-allinone-3.26/ns-3.26/src/dqc/model/thirdparty/logging
```
