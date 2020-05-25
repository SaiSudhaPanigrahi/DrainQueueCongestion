/*test quic bbr on ns3 platform*/
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/dqc-module.h"
#include <string>
#include <iostream>
using namespace ns3;
using namespace dqc;
const uint32_t TOPO_DEFAULT_BW     = 3000000;    // in bps: 3Mbps
const uint32_t TOPO_DEFAULT_PDELAY =      100;    // in ms:   100ms
const uint32_t TOPO_DEFAULT_QDELAY =     300;    // in ms:  300ms
const uint32_t DEFAULT_PACKET_SIZE = 1000;
static int ip=1;
static NodeContainer BuildExampleTopo (uint64_t bps,
                                       uint32_t msDelay,
                                       uint32_t msQdelay)
{
    NodeContainer nodes;
    nodes.Create (2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", DataRateValue  (DataRate (bps)));
    pointToPoint.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (msDelay)));
    auto bufSize = std::max<uint32_t> (DEFAULT_PACKET_SIZE, bps * msQdelay / 8000);
    pointToPoint.SetQueue ("ns3::DropTailQueue",
                           "Mode", StringValue ("QUEUE_MODE_BYTES"),
                           "MaxBytes", UintegerValue (bufSize));
    NetDeviceContainer devices = pointToPoint.Install (nodes);

    InternetStackHelper stack;
    stack.Install (nodes);
    Ipv4AddressHelper address;
	std::string nodeip="10.1."+std::to_string(ip)+".0";
    ip++;
    address.SetBase (nodeip.c_str(), "255.255.255.0");
    address.Assign (devices);

    // Uncomment to capture simulated traffic
    // pointToPoint.EnablePcapAll ("rmcat-example");

    // disable tc for now, some bug in ns3 causes extra delay
    TrafficControlHelper tch;
    tch.Uninstall (devices);

	/*std::string errorModelType = "ns3::RateErrorModel";
  	ObjectFactory factory;
  	factory.SetTypeId (errorModelType);
  	Ptr<ErrorModel> em = factory.Create<ErrorModel> ();
	devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));*/


    return nodes;
}
static void InstallDqc( dqc::CongestionControlType cc_type,
                        Ptr<Node> sender,
                        Ptr<Node> receiver,
						uint16_t send_port,
                        uint16_t recv_port,
                        float startTime,
                        float stopTime,
						DqcTrace *trace,
                        uint32_t max_bps=0,uint32_t emucons=1)
{
    Ptr<DqcSender> sendApp = CreateObject<DqcSender> (cc_type);
    //Ptr<DqcDelayAckReceiver> recvApp = CreateObject<DqcDelayAckReceiver>();
	Ptr<DqcReceiver> recvApp = CreateObject<DqcReceiver>();
   	sender->AddApplication (sendApp);
    receiver->AddApplication (recvApp);
	sendApp->SetNumEmulatedConnections(emucons);
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
		//sendApp->SetSentSeqTraceFuc(MakeCallback(&DqcTrace::OnSentSeq,trace));
		recvApp->SetOwdTraceFuc(MakeCallback(&DqcTrace::OnOwd,trace));
	}	
}
static void InstallTcp(
                         Ptr<Node> sender,
                         Ptr<Node> receiver,
                         uint16_t port,
                         float startTime,
                         float stopTime
)
{
    // configure TCP source/sender/client
    auto serverAddr = receiver->GetObject<Ipv4> ()->GetAddress (1,0).GetLocal ();
    BulkSendHelper source{"ns3::TcpSocketFactory",
                           InetSocketAddress{serverAddr, port}};
    // Set the amount of data to send in bytes. Zero is unlimited.
    source.SetAttribute ("MaxBytes", UintegerValue (0));
    source.SetAttribute ("SendSize", UintegerValue (DEFAULT_PACKET_SIZE));

    auto clientApps = source.Install (sender);
    clientApps.Start (Seconds (startTime));
    clientApps.Stop (Seconds (stopTime));

    // configure TCP sink/receiver/server
    PacketSinkHelper sink{"ns3::TcpSocketFactory",
                           InetSocketAddress{Ipv4Address::GetAny (), port}};
    auto serverApps = sink.Install (receiver);
    serverApps.Start (Seconds (startTime));
    serverApps.Stop (Seconds (stopTime));	
	
}
static double simDuration=300;
uint16_t sendPort=5432;
uint16_t recvPort=5000;
float appStart=0.0;
float appStop=simDuration;
int main(int argc, char *argv[]){
	Config::SetDefault ("ns3::RateErrorModel::ErrorRate", DoubleValue (0.01));
	Config::SetDefault ("ns3::RateErrorModel::ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));

	Config::SetDefault ("ns3::BurstErrorModel::ErrorRate", DoubleValue (0.01));
	Config::SetDefault ("ns3::BurstErrorModel::BurstSize", StringValue ("ns3::UniformRandomVariable[Min=1|Max=3]"));
    //std::string filename("error.log");
    //std::ios::openmode filemode=std::ios_base::out;
    //GlobalStream::Create(filename,filemode);
    //LogComponentEnable("dqcsender",LOG_LEVEL_ALL);
	//LogComponentEnable("queue_limit",LOG_LEVEL_ALL);
    //LogComponentEnable("proto_connection",LOG_LEVEL_ALL);
    //LogComponentEnable("dqcreceiver",LOG_LEVEL_ALL);
	//LogComponentEnable("dqcdelayackreceiver",LOG_LEVEL_ALL);
	ns3::LogComponentEnable("proto_pacing",LOG_LEVEL_ALL);
	uint64_t linkBw   = TOPO_DEFAULT_BW;
    uint32_t msDelay  = TOPO_DEFAULT_PDELAY;
    uint32_t msQDelay = TOPO_DEFAULT_QDELAY;
    uint32_t max_bps=0;
	std::string cc_tmp("bbr");
	std::string cc_name;
	CommandLine cmd;
    std::string instance=std::string("1");
    cmd.AddValue ("it", "instacne", instance);
	cmd.AddValue ("cc", "cctype", cc_tmp);
    cmd.Parse (argc, argv);
	cc_name="_"+cc_tmp+"_";
    if(instance==std::string("1")){
        linkBw=3000000;
        msDelay=50;
        msQDelay=100;
    }else if(instance==std::string("2")){
        linkBw=3000000;
        msDelay=50;
        msQDelay=200;        
    }else if(instance==std::string("3")){
        linkBw=3000000;
        msDelay=100;
        msQDelay=300;        
    }else if(instance==std::string("4")){
        linkBw=4000000;
        msDelay=50;
        msQDelay=100;
    }else if(instance==std::string("5")){
        linkBw=4000000;
        msDelay=50;
        msQDelay=200;        
    }else if(instance==std::string("6")){
        linkBw=6000000;
        msDelay=50;
        msQDelay=200;        
    }else if(instance==std::string("7")){
        linkBw=6000000;
        msDelay=100;
        msQDelay=300;        
    }else if(instance==std::string("8")){
        linkBw=8000000;
        msDelay=100;
        msQDelay=200;        
    }else if(instance==std::string("9")){
        linkBw=8000000;
        msDelay=100;
        msQDelay=300;        
    }else if(instance==std::string("10")){
        linkBw=10000000;
        msDelay=50;
        msQDelay=100;        
    }else if(instance==std::string("11")){
        linkBw=10000000;
        msDelay=50;
        msQDelay=150;        
    }else if(instance==std::string("12")){
        linkBw=12000000;
        msDelay=100;
        msQDelay=200;        
    }else if(instance==std::string("13")){
        linkBw=12000000;
        msDelay=100;
        msQDelay=300;        
    }else if(instance==std::string("14")){
        linkBw=15000000;
        msDelay=50;
        msQDelay=150;        
    }
 	dqc::CongestionControlType cc=kQueueLimit;
	if(cc_tmp==std::string("bbr")){
		cc=kBBR;
	}else if(cc_tmp==std::string("cubic")){
		cc=kCubicBytes;
	}else if(cc_tmp==std::string("reno")){
		cc=kRenoBytes;
	}else if(cc_tmp==std::string("vegas")){
		cc=kVegas;
	}else if(cc_tmp==std::string("copa")){
		cc=kCopa;
		std::cout<<cc_tmp<<std::endl;
	}else if(cc_tmp==std::string("pcc")){
		cc=kPCC;
		std::cout<<cc_tmp<<std::endl;
	}else if(cc_tmp==std::string("viva")){
		cc=kVivace;
		std::cout<<cc_tmp<<std::endl;
	}else if(cc_tmp==std::string("webviva")){
		cc=kWebRTCVivace;
		std::cout<<cc_tmp<<std::endl;
	}else{
		cc=kQueueLimit;
	}
    NodeContainer nodes = BuildExampleTopo (linkBw, msDelay, msQDelay);
	int test_pair=1;
	DqcTrace trace1;
	std::string log_common="dqc_"+instance+cc_name;
	std::string log=log_common+std::to_string(test_pair);
	trace1.Log(log,DqcTraceEnable::E_DQC_OWD|DqcTraceEnable::E_DQC_BW);
	test_pair++;
	InstallDqc(cc,nodes.Get(0),nodes.Get(1),sendPort,recvPort,appStart,appStop,&trace1,max_bps);
	DqcTrace trace2;
	log=log_common+std::to_string(test_pair);
	trace2.Log(log,DqcTraceEnable::E_DQC_OWD|DqcTraceEnable::E_DQC_BW);
	test_pair++;
	InstallDqc(cc,nodes.Get(0),nodes.Get(1),sendPort+1,recvPort+1,appStart,appStop,&trace2,max_bps);

	DqcTrace trace3;
	log=log_common+std::to_string(test_pair);
	trace3.Log(log,DqcTraceEnable::E_DQC_OWD|DqcTraceEnable::E_DQC_BW);
	test_pair++;
	InstallDqc(kRenoBytes,nodes.Get(0),nodes.Get(1),sendPort+2,recvPort+2,appStart,appStop,&trace3,max_bps);
    Simulator::Stop (Seconds(simDuration));
    Simulator::Run ();
    Simulator::Destroy();	
    return 0;
}
