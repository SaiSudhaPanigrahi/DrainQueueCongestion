/** Network topology
 *
 *    10Mb/s, 2ms                            10Mb/s, 4ms
 * n0--------------|                    |---------------n4
 *                 |   1.5Mbps/s, 20ms  |
 *                 n2------------------n3
 *    10Mb/s, 3ms  |                    |    10Mb/s, 5ms
 * n1--------------|                    |---------------n5
 *
 *
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/dqc-module.h"
#include "ns3/log.h"
#include<stdio.h>
#include<iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>
#include <memory>
using namespace ns3;
using namespace dqc;
using namespace std;
NS_LOG_COMPONENT_DEFINE ("bbr-rtt");

uint32_t checkTimes;
double avgQueueSize;

// The times
double global_start_time;
double global_stop_time;
double sink_start_time;
double sink_stop_time;
double client_start_time;
double client_stop_time;

NodeContainer n0n2;
NodeContainer n2n3;
NodeContainer n3n4;
NodeContainer n1n2;
NodeContainer n3n5;

Ipv4InterfaceContainer i0i2;
Ipv4InterfaceContainer i1i2;
Ipv4InterfaceContainer i2i3;
Ipv4InterfaceContainer i3i4;
Ipv4InterfaceContainer i3i5;

typedef struct
{
uint64_t bps;
uint32_t msDelay;
uint32_t msQdelay;	
}link_config_t;
//unrelated topology
/*
   L3      L1      L4
configuration same as the above dumbbell topology
n0--L0--n2--L1--n3--L2--n4
n1--L3--n2--L1--n3--L4--n5
*/
link_config_t p4p[]={
[0]={100*1000000,10,150},
[1]={5*1000000,10,150},
[2]={100*1000000,10,150},
[3]={100*1000000,20,150},
[4]={100*1000000,20,150},
};
const uint32_t TOPO_DEFAULT_BW     = 5000000;    // in bps: 3Mbps
const uint32_t TOPO_DEFAULT_PDELAY =      10;    // in ms:   100ms
const uint32_t TOPO_DEFAULT_QDELAY =     150;    // in ms:  300ms
static void InstallDqc( dqc::CongestionControlType cc_type,
                        Ptr<Node> sender,
                        Ptr<Node> receiver,
						uint16_t send_port,
                        uint16_t recv_port,
                        float startTime,
                        float stopTime,
						DqcTrace *trace,
                        uint32_t max_bps=0
)
{
    Ptr<DqcSender> sendApp = CreateObject<DqcSender> (cc_type);
	Ptr<DqcReceiver> recvApp = CreateObject<DqcReceiver>();
   	sender->AddApplication (sendApp);
    receiver->AddApplication (recvApp);
    Ptr<Ipv4> ipv4 = receiver->GetObject<Ipv4> ();
	Ipv4Address receiverIp = ipv4->GetAddress (1, 0).GetLocal ();
	recvApp->Bind(recv_port);
	sendApp->Bind(send_port);
	sendApp->ConfigurePeer(receiverIp,recv_port);
    sendApp->SetStartTime (Seconds (startTime));
    sendApp->SetStopTime (Seconds (stopTime));
    recvApp->SetStartTime (Seconds (startTime));
    recvApp->SetStopTime (Seconds (stopTime));
    if(max_bps>0){
        sendApp->SetMaxBandwidth(max_bps);
    }
	if(trace){
		sendApp->SetBwTraceFuc(MakeCallback(&DqcTrace::OnBw,trace));
		recvApp->SetOwdTraceFuc(MakeCallback(&DqcTrace::OnOwd,trace));
	}	
}
static double simDuration=200;
uint16_t sendPort=5432;
uint16_t recvPort=5000;
float appStart=0.0;
float appStop=simDuration;
int main (int argc, char *argv[]){
    LogComponentEnable("dqcsender",LOG_LEVEL_ALL);
    LogComponentEnable("proto_connection",LOG_LEVEL_ALL);
	CommandLine cmd;
    std::string instance=std::string("1");
    std::string cc_tmp("cubicplus");
	std::string loss_str("0");
    cmd.AddValue ("it", "instacne", instance);
	cmd.AddValue ("cc", "cctype", cc_tmp);
    cmd.Parse (argc, argv);
    uint64_t linkBw   = TOPO_DEFAULT_BW;
    uint32_t msDelay  = TOPO_DEFAULT_PDELAY;
    std::string cc_name;
    cc_name="_"+cc_tmp+"rtt_";
    if(instance==std::string("1")){
        linkBw=4000000;
        msDelay=10;
    }else if(instance==std::string("2")){
        linkBw=4000000;
        msDelay=20;     
    }else if(instance==std::string("3")){
        linkBw=4000000;
        msDelay=30;     
    }else if(instance==std::string("4")){
        linkBw=6000000;
        msDelay=10;     
    }else if(instance==std::string("5")){
        linkBw=6000000;
        msDelay=20;     
    }else if(instance==std::string("6")){
        linkBw=6000000;
        msDelay=30;     
    }else if(instance==std::string("7")){
        linkBw=8000000;
        msDelay=10;
    }else if(instance==std::string("8")){
        linkBw=8000000;
        msDelay=20;
    }else if(instance==std::string("9")){
        linkBw=8000000;
        msDelay=30;
    }else{
        linkBw=3000000;
        msDelay=10;        
    }
    p4p[1].bps=linkBw;
    p4p[1].msDelay=msDelay;
    uint32_t owd1=p4p[0].msDelay+p4p[1].msDelay+p4p[2].msDelay;
    uint32_t owd2=p4p[3].msDelay+p4p[1].msDelay+p4p[4].msDelay;
    uint32_t owd=std::max(owd1,owd2);
    uint32_t msQdelay=owd*3;
    for(size_t i=0;i<sizeof(p4p)/sizeof(p4p[0]);i++){
        p4p[i].msQdelay=msQdelay;
    }
  NodeContainer c;
  c.Create (6);
  Names::Add ( "N0", c.Get (0));
  Names::Add ( "N1", c.Get (1));
  Names::Add ( "N2", c.Get (2));
  Names::Add ( "N3", c.Get (3));
  Names::Add ( "N4", c.Get (4));
  Names::Add ( "N5", c.Get (5));
  n0n2 = NodeContainer (c.Get (0), c.Get (2));
  n1n2 = NodeContainer (c.Get (1), c.Get (2));
  n2n3 = NodeContainer (c.Get (2), c.Get (3));
  n3n4 = NodeContainer (c.Get (3), c.Get (4));
  n3n5 = NodeContainer (c.Get (3), c.Get (5));
  uint32_t meanPktSize = 1500;
  link_config_t *config=p4p;
  uint32_t bufSize=0;	

  InternetStackHelper internet;
  internet.Install (c);

  NS_LOG_INFO ("Create channels");
  PointToPointHelper p2p;
  TrafficControlHelper tch;
  //L0
  bufSize =config[0].bps * config[0].msQdelay/8000;
  p2p.SetQueue ("ns3::DropTailQueue",
                "Mode", StringValue ("QUEUE_MODE_BYTES"),
                "MaxBytes", UintegerValue (bufSize));
  p2p.SetDeviceAttribute ("DataRate", DataRateValue(DataRate (config[0].bps)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (config[0].msDelay)));
  NetDeviceContainer devn0n2 = p2p.Install (n0n2);
  //L3
  bufSize =config[3].bps * config[3].msQdelay/8000;
  p2p.SetQueue ("ns3::DropTailQueue",
                "Mode", StringValue ("QUEUE_MODE_BYTES"),
                "MaxBytes", UintegerValue (bufSize));
  p2p.SetDeviceAttribute ("DataRate", DataRateValue(DataRate (config[3].bps)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (config[3].msDelay)));
  NetDeviceContainer devn1n2 = p2p.Install (n1n2);
  //L1
  bufSize =config[1].bps * config[1].msQdelay/8000;
  p2p.SetQueue ("ns3::DropTailQueue",
                "Mode", StringValue ("QUEUE_MODE_BYTES"),
                "MaxBytes", UintegerValue (bufSize)); 
  p2p.SetDeviceAttribute ("DataRate", DataRateValue(DataRate (config[1].bps)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (config[1].msDelay)));
  NetDeviceContainer devn2n3 = p2p.Install (n2n3);
  //L2
  bufSize =config[2].bps * config[2].msQdelay/8000;
  p2p.SetQueue ("ns3::DropTailQueue",
                "Mode", StringValue ("QUEUE_MODE_BYTES"),
                "MaxBytes", UintegerValue (bufSize));
  p2p.SetDeviceAttribute ("DataRate", DataRateValue(DataRate (config[2].bps)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (config[2].msDelay)));
  NetDeviceContainer devn3n4 = p2p.Install (n3n4);
  //L4
  bufSize =config[4].bps * config[4].msQdelay/8000;
  p2p.SetQueue ("ns3::DropTailQueue",
                "Mode", StringValue ("QUEUE_MODE_BYTES"),
                "MaxBytes", UintegerValue (bufSize));
  p2p.SetDeviceAttribute ("DataRate", DataRateValue(DataRate (config[4].bps)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (config[4].msDelay)));
  NetDeviceContainer devn3n5 = p2p.Install (n3n5);

  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  i0i2 = ipv4.Assign (devn0n2);
  tch.Uninstall (devn0n2);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  i1i2 = ipv4.Assign (devn1n2);
  tch.Uninstall (devn1n2);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  i2i3 = ipv4.Assign (devn2n3);
  tch.Uninstall (devn2n3);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  i3i4 = ipv4.Assign (devn3n4);
  tch.Uninstall (devn3n4);

  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  i3i5 = ipv4.Assign (devn3n5);
  tch.Uninstall (devn3n5);

  // Set up the routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  dqc::CongestionControlType cc=kBBRPlus;
	if(cc_tmp==std::string("bbr")){
		cc=kBBR;
	}else if(cc_tmp==std::string("bbrd")){
		cc=kBBR; //drain to target
	}else if(cc_tmp==std::string("bbrplus")){
		cc=kBBRPlus;
	}else if(cc_tmp==std::string("bbrrand")){
		cc=kBBRRand;
	}else if(cc_tmp==std::string("bbrv2")){
		cc=kBBRv2;
	}else if(cc_tmp==std::string("cubic")){
		cc=kCubicBytes;
	}else if(cc_tmp==std::string("westwood")){
		cc=kWestwood;
		std::cout<<cc_tmp<<std::endl;
	}else if(cc_tmp==std::string("cubicplus")){
		cc=kCubicPlus;
	}else if(cc_tmp==std::string("reno")){
		cc=kRenoBytes;	
	}else if(cc_tmp==std::string("hsr")){
		cc=kHighSpeedRail;
	}else if(cc_tmp==std::string("tsu")){
		cc=kTsunami;
	}
	std::cout<<cc_tmp<<std::endl;
  uint32_t max_bps=0;
	int test_pair=1;
	DqcTrace trace1;
	std::string log=instance+cc_name+std::to_string(test_pair);
	trace1.Log(log,DqcTraceEnable::E_DQC_OWD|DqcTraceEnable::E_DQC_BW);
	test_pair++;
    InstallDqc(cc,c.Get(0),c.Get(4),sendPort,recvPort,appStart,appStop,&trace1,max_bps);

	DqcTrace trace2;
	log=instance+cc_name+std::to_string(test_pair);
	trace2.Log(log,DqcTraceEnable::E_DQC_OWD|DqcTraceEnable::E_DQC_BW);
	test_pair++;
	InstallDqc(cc,c.Get(1),c.Get(5),sendPort+1,recvPort+1,appStart,appStop,&trace2,max_bps);
    Simulator::Stop (Seconds(simDuration));
    Simulator::Run ();
    Simulator::Destroy();

}

