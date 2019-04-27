/*
Application Detail:
Analyse and compare TCP Reno, TCP Westwood, and TCP Fack (i.e. Reno TCP with "forward
acknowledgment") performance. Select a Dumbbell topology with two routers R1 and R2 connected by a
(10 Mbps, 50 ms) wired link. Each of the routers is connected to 3 hosts i.e., H1 to H3 (i.e. senders) are
connected to R1 and H4 to H6 (i.e. receivers) are connected to R2. The hosts are attached with (100 Mbps,
20ms) links. Both the routers use drop-tail queues with queue size set according to bandwidth-delay product.
Senders (i.e. H1, H2 and H3) are attached with TCP Reno, TCP Westwood, and TCP Fack agents respectively.
Choose a packet size of 1.2KB and perform the following task. Make appropriate assumptions wherever
necessary.
a. Start only one flow and analyse the throughput over sufficiently long duration. Mention how do you
select the duration. Plot of evolution of congestion window over time. Perform this experiment
with flows attached to all the three sending agents.
b. Next, start 2 other flows sharing the bottleneck while the first one is in progress and measure the
throughput (in Kbps) of each flow. Plot the throughput and congestion window of each flow at
steady-state. What is the maximum throughput of each flow? 
c. Measure the congestion loss and goodput over the duration of the experiment for each flow. 
Implementation detail:
		 _								_
		|	H1------+		+------H4	 |
		|			|		|			 |
Senders	|	H2------R1------R2-----H5	 |	Receivers
		|			|		|			 |
		|_	H3------+		+------H6	_|
	Representation in code:
	H1(n0), H2(n1), H3(n2), H4(n3), H5(n4), H6(n5), R1(n6), R2(n7) :: n stands for node
	Dumbbell topology is used with 
	H1, H2, H3 on left side of dumbbell,
	H4, H5, H6 on right side of dumbbell,
	and routers R1 and R2 form the bridge of dumbbell.
	H1 is attached with TCP Reno agent.
	H2 is attached with TCP Westfood agent.
	H3 is attached with TCP Fack agent.
	Links:
	H1R1/H2R1/H3R1/H4R2/H5R2/H6R2: P2P with 100Mbps and 20ms.
	R1R2: (dumbbell bridge) P2P with 10Mbps and 50ms.
	packet size: 1.2KB.
	Number of packets decided by Bandwidth delay product:
	i.e. #packets = Bandwidth*Delay(in bits)
	Therefore, max #packets (HiRj) = 100Mbps*20ms = 2000000
	and max #packets (R1R2) = 10Mbps*50ms = 500000
*/
#include <string>
#include <fstream>
#include <cstdlib>
#include <map>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/gnuplot.h"

typedef uint32_t int;

using namespace ns3;

#define ERROR 0.000001

NS_LOG_COMPONENT_DEFINE ("App6");

class APP: public Application {
	private:
		virtual void StartApplication(void);
		virtual void StopApplication(void);

		void ScheduleTx(void);
		void SendPacket(void);

		Ptr<Socket>     mSocket;
		Address         mPeer;
		uint32_t        mPacketSize;
		uint32_t        mNPackets;
		DataRate        mDataRate;
		EventId         mSendEvent;
		bool            mRunning;
		uint32_t        mPacketsSent;

	public:
		APP();
		virtual ~APP();

		void Setup(Ptr<Socket> socket, Address address, int packetSize, int nPackets, DataRate dataRate);
		void ChangeRate(DataRate newRate);
		void recv(int numBytesRcvd);

};

APP::APP(): mSocket(0),
		    mPeer(),
		    mPacketSize(0),
		    mNPackets(0),
		    mDataRate(0),
		    mSendEvent(),
		    mRunning(false),
		    mPacketsSent(0) {
}

APP::~APP() {
	mSocket = 0;
}

void APP::Setup(Ptr<Socket> socket, Address address, int packetSize, int nPackets, DataRate dataRate) {
	mSocket = socket;
	mPeer = address;
	mPacketSize = packetSize;
	mNPackets = nPackets;
	mDataRate = dataRate;
}

void APP::StartApplication() {
	mRunning = true;
	mPacketsSent = 0;
	mSocket->Bind();
	mSocket->Connect(mPeer);
	SendPacket();
}

void APP::StopApplication() {
	mRunning = false;
	if(mSendEvent.IsRunning()) {
		Simulator::Cancel(mSendEvent);
	}
	if(mSocket) {
		mSocket->Close();
	}
}

void APP::SendPacket() {
	Ptr<Packet> packet = Create<Packet>(mPacketSize);
	mSocket->Send(packet);

	if(++mPacketsSent < mNPackets) {
		ScheduleTx();
	}
}

void APP::ScheduleTx() {
	if (mRunning) {
		Time tNext(Seconds(mPacketSize*8/static_cast<double>(mDataRate.GetBitRate())));
		mSendEvent = Simulator::Schedule(tNext, &APP::SendPacket, this);
		//double tVal = Simulator::Now().GetSeconds();
		//if(tVal-int(tVal) >= 0.99)
		//	std::cout << Simulator::Now ().GetSeconds () << "\t" << mPacketsSent << std::endl;
	}
}

void APP::ChangeRate(DataRate newrate) {
	mDataRate = newrate;
	return;
}

static void CwndChange(Ptr<OutputStreamWrapper> stream, double startTime, int oldCwnd, int newCwnd) {
	*stream->GetStream() << Simulator::Now ().GetSeconds () - startTime << "\t" << newCwnd << std::endl;
}

std::map<int, int> mapDrop;
static void packetDrop(Ptr<OutputStreamWrapper> stream, double startTime, int myId) {
	*stream->GetStream() << Simulator::Now ().GetSeconds () - startTime << "\t" << std::endl;
	if(mapDrop.find(myId) == mapDrop.end()) {
		mapDrop[myId] = 0;
	}
	mapDrop[myId]++;
}


void IncRate(Ptr<APP> app, DataRate rate) {
	app->ChangeRate(rate);
	return;
}

std::map<Address, double> mapBytesReceived;
std::map<std::string, double> mapBytesReceivedIPV4, mapMaxThroughput;
static double lastTimePrint = 0, lastTimePrintIPV4 = 0;
double printGap = 0;

void ReceivedPacket(Ptr<OutputStreamWrapper> stream, double startTime, std::string context, Ptr<const Packet> p, const Address& addr){
	double timeNow = Simulator::Now().GetSeconds();

	if(mapBytesReceived.find(addr) == mapBytesReceived.end())
		mapBytesReceived[addr] = 0;
	mapBytesReceived[addr] += p->GetSize();
	double kbps_ = (((mapBytesReceived[addr] * 8.0) / 1024)/(timeNow-startTime));
	if(timeNow - lastTimePrint >= printGap) {
		lastTimePrint = timeNow;
		*stream->GetStream() << timeNow-startTime << "\t" <<  kbps_ << std::endl;
	}
}

void ReceivedPacketIPV4(Ptr<OutputStreamWrapper> stream, double startTime, std::string context, Ptr<const Packet> p, Ptr<Ipv4> ipv4, int interface) {
	double timeNow = Simulator::Now().GetSeconds();

	if(mapBytesReceivedIPV4.find(context) == mapBytesReceivedIPV4.end())
		mapBytesReceivedIPV4[context] = 0;
	if(mapMaxThroughput.find(context) == mapMaxThroughput.end())
		mapMaxThroughput[context] = 0;
	mapBytesReceivedIPV4[context] += p->GetSize();
	double kbps_ = (((mapBytesReceivedIPV4[context] * 8.0) / 1024)/(timeNow-startTime));
	if(timeNow - lastTimePrintIPV4 >= printGap) {
		lastTimePrintIPV4 = timeNow;
		*stream->GetStream() << timeNow-startTime << "\t" <<  kbps_ << std::endl;
		if(mapMaxThroughput[context] < kbps_)
			mapMaxThroughput[context] = kbps_;
	}
}


Ptr<Socket> uniFlow(Address sinkAddress, 
					int sinkPort, 
					std::string tcpVariant, 
					Ptr<Node> hostNode, 
					Ptr<Node> sinkNode, 
					double startTime, 
					double stopTime,
					int packetSize,
					int numPackets,
					std::string dataRate,
					double appStartTime,
					double appStopTime) {

	if(tcpVariant.compare("TcpReno") == 0) {
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpReno::GetTypeId()));
	} else if(tcpVariant.compare("TcpWestwood") == 0) {
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
	} else if(tcpVariant.compare("TcpFack") == 0) {
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpTahoe::GetTypeId()));
	} else {
		fprintf(stderr, "Invalid TCP version\n");
		exit(EXIT_FAILURE);
	}
	PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
	ApplicationContainer sinkApps = packetSinkHelper.Install(sinkNode);
	sinkApps.Start(Seconds(startTime));
	sinkApps.Stop(Seconds(stopTime));

	Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(hostNode, TcpSocketFactory::GetTypeId());
	

	Ptr<APP> app = CreateObject<APP>();
	app->Setup(ns3TcpSocket, sinkAddress, packetSize, numPackets, DataRate(dataRate));
	hostNode->AddApplication(app);
	app->SetStartTime(Seconds(appStartTime));
	app->SetStopTime(Seconds(appStopTime));

	return ns3TcpSocket;
}

struct TopologyParam
{
	std::string bandwidth_hostToRouter;
	std::string delay_hostToRouter;
	std::string bandwidth_routerToRouter;
	std::string delay_routerToRouter;

	int packetSize;
	int queueSizeHR;
	int queueSizeRR;

	int numSender;
	int numRecv;
	int numRouters;

	double errorP;

	TopologyParam();
};

TopologyParam::TopologyParam() {
	this -> bandwidth_hostToRouter = "100Mbps";
	this -> delay_hostToRouter = "20ms";
	this -> bandwidth_routerToRouter = "10Mbps";
	this -> delay_routerToRouter = "50ms";

	this -> packetSize = 1.3*1024;		// 1.3KB
	this -> queueSizeHR = (100*1000*20)/this->packetSize;	// #packets_queue_HR = (100Mbps)*(20ms)/packetSize
	this -> queueSizeRR = (10*1000*50)/this->packetSize;	// #packets_queue_RR = (10Mbps)*(50ms)/packetSize

	this -> numSender = 3;
	this -> numRecv = 3;
	this -> numRouters = 2;

	this -> errorP = ERROR;
}

class DumbbellTopology
{
private:
	PointToPointHelper pointToPointRouter;
	PointToPointHelper pointToPointLeaf;
	Ptr<RateErrorModel> errorModel;
	NodeContainer routers, senders, receivers;
	NetDeviceContainer routerDevices;
	NetDeviceContainer leftRouterDevices, rightRouterDevices, senderDevices, receiverDevices;
	InternetStackHelper stack;
	Ipv4AddressHelper routerIP, senderIP, receiverIP;
	Ipv4InterfaceContainer routerIFC, senderIFCs, receiverIFCs, leftRouterIFCs, rightRouterIFCs;

public:
	void setConnections(TopologyParam topologyParams);
	void createNodes(TopologyParam topologyParams);
	void setNetDevices(TopologyParam topologyParams);
	void installInternetStack(TopologyParam topologyParams);
	void addIpAddrToNodes(TopologyParam topologyParams);
	void addIpAddrToNetDevices(TopologyParam topologyParams);
}

//Creating channel without IP address
/*
	SetDeviceAttribute: sets attributes of pointToPointNetDevice
	DataRate: Bandwidth

	SetChannelAttribute: sets attributes of pointToPointChannel
	Delay: Transmission delay through the channel

	SetQueue: sets attribute of a queue say droptailqueue
	MaxPackets: The maximum number of packets accepted by this DropTailQueue.
*/
void DumbbellTopology::setConnections(TopologyParam topologyParams) {
	this->pointToPointRouter.SetDeviceAttribute("DataRate", StringValue(topologyParams.bandwidth_hostToRouter));
	this->pointToPointRouter.SetChannelAttribute("Delay", StringValue(topologyParams.delay_hostToRouter));
	this->pointToPointRouter.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(topologyParams.queueSizeHR));

	this->pointToPointLeaf.SetDeviceAttribute("DataRate", StringValue(topologyParams.bandwidth_routerToRouter));
	this->pointToPointLeaf.SetChannelAttribute("Delay", StringValue(topologyParams.delay_routerToRouter));
	this->pointToPointLeaf.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(topologyParams.queueSizeRR));
}

void DumbbellTopology::createNodes(TopologyParam topologyParams) {
	//Create n nodes and append pointers to them to the end of this NodeContainer.
	this->routers.Create(topologyParams.numRouters);
	this->senders.Create(topologyParams.numSender);
	this->receivers.Create(topologyParams.numRecv);
}

/*
	p2pHelper.Install:
	This method creates a ns3::PointToPointChannel with the attributes configured 
	by PointToPointHelper::SetChannelAttribute, then, for each node in the input container,
	we create a ns3::PointToPointNetDevice with the requested attributes, 
	a queue for this ns3::NetDevice, and associate the resulting ns3::NetDevice 
	with the ns3::Node and ns3::PointToPointChannel.
*/
void DumbbellTopology::setNetDevices(TopologyParam topologyParams) {

	this->routerDevices = this->pointToPointLeaf.Install(this->routers);
	
	//Adding links
	std::cout << "Adding links" << std::endl;
	for (int i = 0; i < this->numSender; ++i) {
		NetDeviceContainer cleft = this->pointToPointRouter.Install(this->routers.Get(0), this->senders.Get(i));
		this->leftRouterDevices.Add(cleft.Get(0));
		this->senderDevices.Add(cleft.Get(1));
		cleft.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(this->errorModel));

		NetDeviceContainer cright = this->pointToPointRouter.Install(this->routers.Get(1), this->receivers.Get(i));
		this->rightRouterDevices.Add(cright.Get(0));
		this->receiverDevices.Add(cright.Get(1));
		cright.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(this->errorModel));
	}
}

//Install Internet Stack
/*
	For each node in the input container, aggregate implementations of 
	the ns3::Ipv4, ns3::Ipv6, ns3::Udp, and, ns3::Tcp classes. 
*/
void DumbbellTopology::installInternetStack(TopologyParam topologyParams) {
	std::cout << "Install internet stack" << std::endl;

	this->stack.Install(this->routers);
	this->stack.Install(this->senders);
	this->stack.Install(this->receivers);
}

//Adding IP addresses
void DumbbellTopology::addIpAddrToNodes(TopologyParam topologyParams) {
	std::cout << "Adding IP addresses" << std::endl;
	this->routerIP = Ipv4AddressHelper("10.3.0.0", "255.255.255.0");	//(network, mask)
	this->senderIP = Ipv4AddressHelper("10.1.0.0", "255.255.255.0");
	this->receiverIP = Ipv4AddressHelper("10.2.0.0", "255.255.255.0");
}

//Adding some errorrate
/*
	Error rate model attributes
	ErrorUnit: The error unit
	ErrorRate: The error rate.
	RanVar: The decision variable attached to this error model.
*/
void DumbbellTopology::setErrorRate(TopologyParam topologyParams) {
	this->errorModel = CreateObjectWithAttributes<RateErrorModel> ("ErrorRate", DoubleValue (topologyParams.errorP));
}

void DumbbellTopology::addIpAddrToNetDevices(TopologyParam topologyParams) {

	//Assign IP addresses to the net devices specified in the container 
	//based on the current network prefix and address base
	this->routerIFC = this->routerIP.Assign(this->routerDevices);

	for (int i = 0; i < this->numSender; ++i) {
		NetDeviceContainer senderDevice;
		senderDevice.Add(this->senderDevices.Get(i));
		senderDevice.Add(this->leftRouterDevices.Get(i));
		Ipv4InterfaceContainer senderIFC = this->senderIP.Assign(senderDevice);
		this->senderIFCs.Add(senderIFC.Get(0));
		this->leftRouterIFCs.Add(senderIFC.Get(1));
		//Increment the network number and reset the IP address counter 
		//to the base value provided in the SetBase method.
		this->senderIP.NewNetwork();

		NetDeviceContainer receiverDevice;
		receiverDevice.Add(this->receiverDevices.Get(i));
		receiverDevice.Add(this->rightRouterDevices.Get(i));
		Ipv4InterfaceContainer receiverIFC = this->receiverIP.Assign(receiverDevice);
		this->receiverIFCs.Add(receiverIFC.Get(0));
		this->rightRouterIFCs.Add(receiverIFC.Get(1));
		this->receiverIP.NewNetwork();
	}
}

void partAC() {
	std::cout << "Part A started..." << std::endl;

	TopologyParam topologyParams;
	DumbbellTopology dumbbellTopology;

	dumbbellTopology.setConnections(topologyParams);
	dumbbellTopology.setErrorRate(topologyParams);
	dumbbellTopology.createNodes(topologyParams);
	dumbbellTopology.setNetDevices(topologyParams);
	dumbbellTopology.installInternetStack(topologyParams);
	dumbbellTopology.addIpAddrToNodes(topologyParams);
	dumbbellTopology.addIpAddrToNetDevices(topologyParams);

	/*
		Measuring Performance of each TCP variant
	*/

	std::cout << "Measuring Performance of each TCP variant..." << std::endl;
	/********************************************************************
	PART (a)
	********************************************************************/
	/********************************************************************
		One flow for each tcp_variant and measure
		1) Throughput for long durations
		2) Evolution of Congestion window
	********************************************************************/
	double durationGap = 100;
	double netDuration = 0;
	int port = 9000;
	int numPackets = 10000000;
	std::string transferSpeed = "400Mbps";	

	//TCP Reno from H1 to H4
	AsciiTraceHelper asciiTraceHelper;
	Ptr<OutputStreamWrapper> stream1CWND = asciiTraceHelper.CreateFileStream("application_6_h1_h4_a.cwnd");
	Ptr<OutputStreamWrapper> stream1PD = asciiTraceHelper.CreateFileStream("application_6_h1_h4_a.congestion_loss");
	Ptr<OutputStreamWrapper> stream1TP = asciiTraceHelper.CreateFileStream("application_6_h1_h4_a.tp");
	Ptr<OutputStreamWrapper> stream1GP = asciiTraceHelper.CreateFileStream("application_6_h1_h4_a.gp");
	Ptr<Socket> ns3TcpSocket1 = uniFlow(InetSocketAddress(receiverIFCs.GetAddress(0), port), port, "TcpReno", senders.Get(0), receivers.Get(0), netDuration, netDuration+durationGap, packetSize, numPackets, transferSpeed, netDuration, netDuration+durationGap);
	ns3TcpSocket1->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, stream1CWND, netDuration));
	ns3TcpSocket1->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, stream1PD, netDuration, 1));

	// Measure PacketSinks
	std::string sink = "/NodeList/5/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sink, MakeBoundCallback(&ReceivedPacket, stream1GP, netDuration));

	std::string sink_ = "/NodeList/5/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sink_, MakeBoundCallback(&ReceivedPacketIPV4, stream1TP, netDuration));

	netDuration += durationGap;


	//TCP Westwood from H2 to H5
	Ptr<OutputStreamWrapper> stream2CWND = asciiTraceHelper.CreateFileStream("application_6_h2_h5_a.cwnd");
	Ptr<OutputStreamWrapper> stream2PD = asciiTraceHelper.CreateFileStream("application_6_h2_h5_a.congestion_loss");
	Ptr<OutputStreamWrapper> stream2TP = asciiTraceHelper.CreateFileStream("application_6_h2_h5_a.tp");
	Ptr<OutputStreamWrapper> stream2GP = asciiTraceHelper.CreateFileStream("application_6_h2_h5_a.gp");
	Ptr<Socket> ns3TcpSocket2 = uniFlow(InetSocketAddress(receiverIFCs.GetAddress(1), port), port, "TcpWestwood", senders.Get(1), receivers.Get(1), netDuration, netDuration+durationGap, packetSize, numPackets, transferSpeed, netDuration, netDuration+durationGap);
	ns3TcpSocket2->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, stream2CWND, netDuration));
	ns3TcpSocket2->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, stream2PD, netDuration, 2));

	sink = "/NodeList/6/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sink, MakeBoundCallback(&ReceivedPacket, stream2GP, netDuration));
	sink_ = "/NodeList/6/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sink_, MakeBoundCallback(&ReceivedPacketIPV4, stream2TP, netDuration));
	netDuration += durationGap;

	//TCP Fack from H3 to H6
	Ptr<OutputStreamWrapper> stream3CWND = asciiTraceHelper.CreateFileStream("application_6_h3_h6_a.cwnd");
	Ptr<OutputStreamWrapper> stream3PD = asciiTraceHelper.CreateFileStream("application_6_h3_h6_a.congestion_loss");
	Ptr<OutputStreamWrapper> stream3TP = asciiTraceHelper.CreateFileStream("application_6_h3_h6_a.tp");
	Ptr<OutputStreamWrapper> stream3GP = asciiTraceHelper.CreateFileStream("application_6_h3_h6_a.gp");
	Ptr<Socket> ns3TcpSocket3 = uniFlow(InetSocketAddress(receiverIFCs.GetAddress(2), port), port, "TcpFack", senders.Get(2), receivers.Get(2), netDuration, netDuration+durationGap, packetSize, numPackets, transferSpeed, netDuration, netDuration+durationGap);
	ns3TcpSocket3->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, stream3CWND, netDuration));
	ns3TcpSocket3->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, stream3PD, netDuration, 3));

	sink = "/NodeList/7/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sink, MakeBoundCallback(&ReceivedPacket, stream3GP, netDuration));
	sink_ = "/NodeList/7/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sink_, MakeBoundCallback(&ReceivedPacketIPV4, stream3TP, netDuration));
	netDuration += durationGap;

	//pointToPointRouter.EnablePcapAll("application_6__a");
	//pointToPointLeaf.EnablePcapAll("application_6_RR_a");

	//Turning on Static Global Routing
	Ipv4GlobalRoutingHelper::PopulateRoutingTables();

	Ptr<FlowMonitor> flowmon;
	FlowMonitorHelper flowmonHelper;
	flowmon = flowmonHelper.InstallAll();
	Simulator::Stop(Seconds(netDuration));
	Simulator::Run();
	flowmon->CheckForLostPackets();

	//Ptr<OutputStreamWrapper> streamTP = asciiTraceHelper.CreateFileStream("application_6_a.tp");
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
	std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();
	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
		Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
		/*
		*streamTP->GetStream()  << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
		*streamTP->GetStream()  << "  Tx Bytes:   " << i->second.txBytes << "\n";
		*streamTP->GetStream()  << "  Rx Bytes:   " << i->second.rxBytes << "\n";
		*streamTP->GetStream()  << "  Time        " << i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds() << "\n";
		*streamTP->GetStream()  << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024  << " Mbps\n";	
		*/
		if(t.sourceAddress == "10.1.0.1") {
			if(mapDrop.find(1)==mapDrop.end())
				mapDrop[1] = 0;
			*stream1PD->GetStream() << "TcpReno Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
			*stream1PD->GetStream()  << "Net Packet Lost: " << i->second.lostPackets << "\n";
			*stream1PD->GetStream()  << "Packet Lost due to buffer overflow: " << mapDrop[1] << "\n";
			*stream1PD->GetStream()  << "Packet Lost due to Congestion: " << i->second.lostPackets - mapDrop[1] << "\n";
			*stream1PD->GetStream() << "Max throughput: " << mapMaxThroughput["/NodeList/5/$ns3::Ipv4L3Protocol/Rx"] << std::endl;
		} else if(t.sourceAddress == "10.1.1.1") {
			if(mapDrop.find(2)==mapDrop.end())
				mapDrop[2] = 0;
			*stream2PD->GetStream() << "Tcp Westwood Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
			*stream2PD->GetStream()  << "Net Packet Lost: " << i->second.lostPackets << "\n";
			*stream2PD->GetStream()  << "Packet Lost due to buffer overflow: " << mapDrop[2] << "\n";
			*stream2PD->GetStream()  << "Packet Lost due to Congestion: " << i->second.lostPackets - mapDrop[2] << "\n";
			*stream2PD->GetStream() << "Max throughput: " << mapMaxThroughput["/NodeList/6/$ns3::Ipv4L3Protocol/Rx"] << std::endl;
		} else if(t.sourceAddress == "10.1.2.1") {
			if(mapDrop.find(3)==mapDrop.end())
				mapDrop[3] = 0;
			*stream3PD->GetStream() << "Tcp Fack Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
			*stream3PD->GetStream()  << "Net Packet Lost: " << i->second.lostPackets << "\n";
			*stream3PD->GetStream()  << "Packet Lost due to buffer overflow: " << mapDrop[3] << "\n";
			*stream3PD->GetStream()  << "Packet Lost due to Congestion: " << i->second.lostPackets - mapDrop[3] << "\n";
			*stream3PD->GetStream() << "Max throughput: " << mapMaxThroughput["/NodeList/7/$ns3::Ipv4L3Protocol/Rx"] << std::endl;
		}
	}

	//flowmon->SerializeToXmlFile("application_6_a.flowmon", true, true);
	std::cout << "Simulation finished" << std::endl;
	Simulator::Destroy();
}


void partBC() {

	/*
	 *	Initialize constraints : Bandwidth-delay, #hosts, DropTailQueue
	 * */
	
	TopologyParam topologyParams;

	/*
	 *	Create Dumbbell topology
	 * */
	DumbbellTopology dumbbellTopology;
	Config::SetDefault("ns3::DropTailQueue::Mode", StringValue("QUEUE_MODE_PACKETS"));

	dumbbellTopology.setConnections(topologyParams);
	dumbbellTopology.setErrorRate(topologyParams);
	dumbbellTopology.createNodes(topologyParams);
	dumbbellTopology.setNetDevices(topologyParams);
	dumbbellTopology.installInternetStack(topologyParams);
	dumbbellTopology.addIpAddrToNodes(topologyParams);
	dumbbellTopology.addIpAddrToNetDevices(topologyParams);
	
	/********************************************************************
	PART (b)
	********************************************************************/
	/********************************************************************
		1)start 2 other flows while one is progress
		and then measure throughput and CWND of each flow at steady state
		2)Also find the max throuhput per flow
	********************************************************************/
	double durationGap = 100;
	double oneFlowStart = 0;
	double otherFlowStart = 20;
	int port = 9000;
	int numPackets = 10000000;
	std::string transferSpeed = "400Mbps";
		
	
	//TCP Reno from H1 to H4
	std::cout << "TCP Reno from H1 to H4" << std::endl;
	AsciiTraceHelper asciiTraceHelper;
	Ptr<OutputStreamWrapper> stream1CWND = asciiTraceHelper.CreateFileStream("application_6_h1_h4_b.cwnd");
	Ptr<OutputStreamWrapper> stream1PD = asciiTraceHelper.CreateFileStream("application_6_h1_h4_b.congestion_loss");
	Ptr<OutputStreamWrapper> stream1TP = asciiTraceHelper.CreateFileStream("application_6_h1_h4_b.tp");
	Ptr<OutputStreamWrapper> stream1GP = asciiTraceHelper.CreateFileStream("application_6_h1_h4_b.gp");
	Ptr<Socket> ns3TcpSocket1 = uniFlow(InetSocketAddress(receiverIFCs.GetAddress(0), port), port, "TcpReno", senders.Get(0), receivers.Get(0), oneFlowStart, oneFlowStart+durationGap, packetSize, numPackets, transferSpeed, oneFlowStart, oneFlowStart+durationGap);
	ns3TcpSocket1->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, stream1CWND, 0));
	ns3TcpSocket1->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, stream1PD, 0, 1));


	std::string sink = "/NodeList/5/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sink, MakeBoundCallback(&ReceivedPacket, stream1GP, 0));
	std::string sink_ = "/NodeList/5/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sink_, MakeBoundCallback(&ReceivedPacketIPV4, stream1TP, 0));

	//TCP Westwood from H2 to H5
	std::cout << "TCP Westwood from H2 to H5" << std::endl;
	Ptr<OutputStreamWrapper> stream2CWND = asciiTraceHelper.CreateFileStream("application_6_h2_h5_b.cwnd");
	Ptr<OutputStreamWrapper> stream2PD = asciiTraceHelper.CreateFileStream("application_6_h2_h5_b.congestion_loss");
	Ptr<OutputStreamWrapper> stream2TP = asciiTraceHelper.CreateFileStream("application_6_h2_h5_b.tp");
	Ptr<OutputStreamWrapper> stream2GP = asciiTraceHelper.CreateFileStream("application_6_h2_h5_b.gp");
	Ptr<Socket> ns3TcpSocket2 = uniFlow(InetSocketAddress(receiverIFCs.GetAddress(1), port), port, "TcpWestwood", senders.Get(1), receivers.Get(1), otherFlowStart, otherFlowStart+durationGap, packetSize, numPackets, transferSpeed, otherFlowStart, otherFlowStart+durationGap);
	ns3TcpSocket2->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, stream2CWND, 0));
	ns3TcpSocket2->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, stream2PD, 0, 2));

	sink = "/NodeList/6/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sink, MakeBoundCallback(&ReceivedPacket, stream2GP, 0));
	sink_ = "/NodeList/6/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sink_, MakeBoundCallback(&ReceivedPacketIPV4, stream2TP, 0));

	//TCP Fack from H3 to H6
	std::cout << "TCP Fack from H3 to H6" << std::endl;
	Ptr<OutputStreamWrapper> stream3CWND = asciiTraceHelper.CreateFileStream("application_6_h3_h6_b.cwnd");
	Ptr<OutputStreamWrapper> stream3PD = asciiTraceHelper.CreateFileStream("application_6_h3_h6_b.congestion_loss");
	Ptr<OutputStreamWrapper> stream3TP = asciiTraceHelper.CreateFileStream("application_6_h3_h6_b.tp");
	Ptr<OutputStreamWrapper> stream3GP = asciiTraceHelper.CreateFileStream("application_6_h3_h6_b.gp");
	Ptr<Socket> ns3TcpSocket3 = uniFlow(InetSocketAddress(receiverIFCs.GetAddress(2), port), port, "TcpFack", senders.Get(2), receivers.Get(2), otherFlowStart, otherFlowStart+durationGap, packetSize, numPackets, transferSpeed, otherFlowStart, otherFlowStart+durationGap);
	ns3TcpSocket3->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, stream3CWND, 0));
	ns3TcpSocket3->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, stream3PD, 0, 3));

	sink = "/NodeList/7/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sink, MakeBoundCallback(&ReceivedPacket, stream3GP, 0));
	sink_ = "/NodeList/7/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sink_, MakeBoundCallback(&ReceivedPacketIPV4, stream3TP, 0));

	//pointToPointRouter.EnablePcapAll("application_6_HR_a");
	//pointToPointLeaf.EnablePcapAll("application_6_RR_a");

	//Turning on Static Global Routing
	std::cout << "Turning on Static Global Routing" << std::endl;
	Ipv4GlobalRoutingHelper::PopulateRoutingTables();

	std::cout << "Monitoring flows..." << std::endl;
	Ptr<FlowMonitor> flowmon;
	FlowMonitorHelper flowmonHelper;
	flowmon = flowmonHelper.InstallAll();
	Simulator::Stop(Seconds(durationGap+otherFlowStart));
	Simulator::Run();
	flowmon->CheckForLostPackets();

	//Ptr<OutputStreamWrapper> streamTP = asciiTraceHelper.CreateFileStream("application_6_b.tp");
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
	std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();
	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
		Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
		/*
		*streamTP->GetStream()  << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
		*streamTP->GetStream()  << "  Tx Bytes:   " << i->second.txBytes << "\n";
		*streamTP->GetStream()  << "  Rx Bytes:   " << i->second.rxBytes << "\n";
		*streamTP->GetStream()  << "  Time        " << i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds() << "\n";
		*streamTP->GetStream()  << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024  << " Mbps\n";	
		*/
		if(t.sourceAddress == "10.1.0.1") {
			if(mapDrop.find(1)==mapDrop.end())
				mapDrop[1] = 0;
			*stream1PD->GetStream() << "TcpReno Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
			*stream1PD->GetStream()  << "Net Packet Lost: " << i->second.lostPackets << "\n";
			*stream1PD->GetStream()  << "Packet Lost due to buffer overflow: " << mapDrop[1] << "\n";
			*stream1PD->GetStream()  << "Packet Lost due to Congestion: " << i->second.lostPackets - mapDrop[1] << "\n";
			*stream1PD->GetStream() << "Max throughput: " << mapMaxThroughput["/NodeList/5/$ns3::Ipv4L3Protocol/Rx"] << std::endl;
		} else if(t.sourceAddress == "10.1.1.1") {
			if(mapDrop.find(2)==mapDrop.end())
				mapDrop[2] = 0;
			*stream2PD->GetStream() << "TcpWestwood Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
			*stream2PD->GetStream()  << "Net Packet Lost: " << i->second.lostPackets << "\n";
			*stream2PD->GetStream()  << "Packet Lost due to buffer overflow: " << mapDrop[2] << "\n";
			*stream2PD->GetStream()  << "Packet Lost due to Congestion: " << i->second.lostPackets - mapDrop[2] << "\n";
			*stream2PD->GetStream() << "Max throughput: " << mapMaxThroughput["/NodeList/6/$ns3::Ipv4L3Protocol/Rx"] << std::endl;
		} else if(t.sourceAddress == "10.1.2.1") {
			if(mapDrop.find(3)==mapDrop.end())
				mapDrop[3] = 0;
			*stream3PD->GetStream() << "TcpFack Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
			*stream3PD->GetStream()  << "Net Packet Lost: " << i->second.lostPackets << "\n";
			*stream3PD->GetStream()  << "Packet Lost due to buffer overflow: " << mapDrop[3] << "\n";
			*stream3PD->GetStream()  << "Packet Lost due to Congestion: " << i->second.lostPackets - mapDrop[3] << "\n";
			*stream3PD->GetStream() << "Max throughput: " << mapMaxThroughput["/NodeList/7/$ns3::Ipv4L3Protocol/Rx"] << std::endl;
		}
	}

	//flowmon->SerializeToXmlFile("application_6_b.flowmon", true, true);
	std::cout << "Simulation finished" << std::endl;
	Simulator::Destroy();

}

int main(int argc, char **argv) {
	
		//partAC();
		partBC();
}
