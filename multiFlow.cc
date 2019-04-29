#include "header.h"

struct ExperimentParameters
{
	std::string rateHR;
	std::string latencyHR;
	std::string rateRR;
	std::string latencyRR;

	int packetSize;
	int queueSizeHR;
	int queueSizeRR;

	int numSender;
	int numReceiver;
	int numRouters;

	double errorParam;

	ExperimentParameters() {
		this -> rateHR = "100Mbps";
		this -> latencyHR = "20ms";
		this -> rateRR = "10Mbps";
		this -> latencyRR = "50ms";

		this -> packetSize = 1.3*1024;		// 1.3KB
		this -> queueSizeHR = (100*1000*20)/this->packetSize;	// #packets_queue_HR = (100Mbps)*(20ms)/packetSize
		this -> queueSizeRR = (10*1000*50)/this->packetSize;	// #packets_queue_RR = (10Mbps)*(50ms)/packetSize

		this -> numSender = 3;
		this -> numReceiver = 3;
		this -> numRouters = 2;

		this -> errorParam = ERROR;
	}
};


/* Connects a router to hosts passed in a NodeContainer, and fills in passed NetDeviceContainers with connected network devices. */
void connectRouterToHosts(Ptr<Node> Router, NodeContainer& Hosts, PointToPointHelper& p2p, NetDeviceContainer& RouterDevices, NetDeviceContainer& HostDevices){
    for(uint i = 0; i < Hosts.GetN(); ++i){
        Ptr<Node> host = Hosts.Get(i);
        NetDeviceContainer p2plink = p2p.Install(Router, host);

        Ptr<NetDevice> routerDevice = p2plink.Get(0);
        RouterDevices.Add(routerDevice);

        Ptr<NetDevice> hostDevice = p2plink.Get(1);
        HostDevices.Add(hostDevice);
    }
}

/* Assigns IPs to the interfaces between hosts and routers. */
void assignIPs(NetDeviceContainer& HostDevices, NetDeviceContainer& RouterDevices, Ipv4AddressHelper& IPHelper, Ipv4InterfaceContainer& HostInterfaces, Ipv4InterfaceContainer& RouterInterfaces){
    assert(HostDevices.GetN() == RouterDevices.GetN());

    for(uint i = 0; i < RouterDevices.GetN(); ++i)
	{
		NetDeviceContainer p2pLink;
		p2pLink.Add(HostDevices.Get(i));
		p2pLink.Add(RouterDevices.Get(i));

		Ipv4InterfaceContainer p2pInterface = IPHelper.Assign(p2pLink);
		HostInterfaces.Add(p2pInterface.Get(0));
		RouterInterfaces.Add(p2pInterface.Get(1));

		// Increments the network number and resets the IP address counter to the base value.
		IPHelper.NewNetwork();
    }
}

int main() 
{
	std::cout << "* PART-2 AND PART-3 STARTED *" << std::endl;
	ExperimentParameters params;
	
	PointToPointHelper p2pHR = configureP2PHelper(params.rateHR, params.latencyHR, std::to_string(params.queueSizeHR)+"p");
	PointToPointHelper p2pRR = configureP2PHelper(params.rateRR, params.latencyRR, std::to_string(params.queueSizeRR)+"p");
	Ptr<RateErrorModel> errorModel = CreateObjectWithAttributes<RateErrorModel> ("ErrorRate", DoubleValue (params.errorParam));

	// Containers for the routers, senders and receivers.
	NodeContainer routers, senders, receivers;

	// Create the required number of hosts and routers, storing them in containers.
	routers.Create(params.numRouters);
	senders.Create(params.numSender);
	receivers.Create(params.numReceiver);

    // Connect routers.
	NetDeviceContainer routerDevices = p2pRR.Install(routers);
	
	// Containers for the network devices on these nodes.
	NetDeviceContainer leftRouterDevices, rightRouterDevices, senderDevices, receiverDevices;

	std::cout << "Adding links...";

    // Connect left router to senders. The NetDeviceContainers are filled in.
    connectRouterToHosts(routers.Get(0), senders, p2pHR, leftRouterDevices, senderDevices);

    // Connect right router to receivers. The NetDeviceContainers are filled in.
    connectRouterToHosts(routers.Get(1), receivers, p2pHR, rightRouterDevices, receiverDevices);

    // Check if network devices created and connected.
    assert(leftRouterDevices.GetN() == (uint) params.numSender);
    assert(leftRouterDevices.GetN() == rightRouterDevices.GetN());
    assert(rightRouterDevices.GetN() == (uint) params.numReceiver);

    // Sets Error Model to corrupt packets randomly.
    int numRouterDevices = leftRouterDevices.GetN();
    for(int i = 0; i < numRouterDevices; ++i){
        Ptr<NetDevice> leftRouterDevice = leftRouterDevices.Get(0);
        leftRouterDevice -> SetAttribute("ReceiveErrorModel", PointerValue(errorModel));

        Ptr<NetDevice> rightRouterDevice = rightRouterDevices.Get(0);
        rightRouterDevice -> SetAttribute("ReceiveErrorModel", PointerValue(errorModel));
    }

	std::cout<< "Topology set up." << std::endl;
	
	//Install Internet Stack
	/*
		For each node in the input container, aggregate implementations of 
		the ns3::Ipv4, ns3::Ipv6, ns3::Udp, and, ns3::Tcp classes. 
	*/
	std::cout << "Installing Internet stack...";
	InternetStackHelper stack;
	stack.Install(routers);
	stack.Install(senders);
	stack.Install(receivers);
	std::cout << "Internet stack installed." << std::endl;


	// Assigning IP addresses. First, set the network prefix and the subnet mask.
	std::cout << "Assigning IP addresses...";
	Ipv4AddressHelper routerIPHelper = Ipv4AddressHelper("172.15.0.0", "255.255.255.0");
	Ipv4AddressHelper senderIPHelper = Ipv4AddressHelper("172.16.0.0", "255.255.255.0");
	Ipv4AddressHelper receiverIPHelper = Ipv4AddressHelper("172.17.0.0", "255.255.255.0");

	Ipv4InterfaceContainer routerIFC, senderIFCs, receiverIFCs, leftRouterIFCs, rightRouterIFCs;

	// Assign IP addresses to the net devices specified in the container based on the current network prefix and address base
	routerIFC = routerIPHelper.Assign(routerDevices);

    assignIPs(senderDevices, leftRouterDevices, senderIPHelper, senderIFCs, leftRouterIFCs);
    assignIPs(receiverDevices, rightRouterDevices, receiverIPHelper, receiverIFCs, rightRouterIFCs);

	std::cout<< "IPs assigned." << std::endl;
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
	int numPackets = 1000000;
	std::string transferSpeed = "400Mbps";
		
	
	//TCP Reno from H1 to H4
	std::cout << "** TCP Hybla from H1 to H4" << std::endl;
	AsciiTraceHelper asciiTraceHelper;
	Ptr<OutputStreamWrapper> h1cw = asciiTraceHelper.CreateFileStream("PartB/DataFiles/data_hybla_b.cw");
	Ptr<OutputStreamWrapper> h1cl = asciiTraceHelper.CreateFileStream("PartB/DataFiles/hybla_b.cl");
	Ptr<OutputStreamWrapper> h1tp = asciiTraceHelper.CreateFileStream("PartB/DataFiles/data_hybla_b.tp");
	Ptr<OutputStreamWrapper> h1gp = asciiTraceHelper.CreateFileStream("PartB/DataFiles/data_hybla_b.gp");
	Ptr<Socket> ns3TcpSocket1 = Simulate(InetSocketAddress(receiverIFCs.GetAddress(0), port), port, "TcpHybla", senders.Get(0), receivers.Get(0), oneFlowStart, oneFlowStart+durationGap, params.packetSize, numPackets, transferSpeed, oneFlowStart, oneFlowStart+durationGap);
	ns3TcpSocket1->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, h1cw, 0));
	ns3TcpSocket1->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, h1cl, 0, 1));


	std::string sink = "/NodeList/5/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sink, MakeBoundCallback(&recvpacket, h1gp, 0));
	std::string sink_ = "/NodeList/5/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sink_, MakeBoundCallback(&recvpacketv4, h1tp, 0));

	//TCP Westwood from H2 to H5
	std::cout << "** TCP WestwoodPlus from H2 to H5" << std::endl;
	Ptr<OutputStreamWrapper> h2cw = asciiTraceHelper.CreateFileStream("PartB/DataFiles/data_westwoodplus_b.cw");
	Ptr<OutputStreamWrapper> h2cl = asciiTraceHelper.CreateFileStream("PartB/DataFiles/westwoodplus_b.cl");
	Ptr<OutputStreamWrapper> h2tp = asciiTraceHelper.CreateFileStream("PartB/DataFiles/data_westwoodplus_b.tp");
	Ptr<OutputStreamWrapper> h2gp = asciiTraceHelper.CreateFileStream("PartB/DataFiles/data_westwoodplus_b.gp");
	Ptr<Socket> ns3TcpSocket2 = Simulate(InetSocketAddress(receiverIFCs.GetAddress(1), port), port, "TcpWestwoodPlus", senders.Get(1), receivers.Get(1), otherFlowStart, otherFlowStart+durationGap, params.packetSize, numPackets, transferSpeed, otherFlowStart, otherFlowStart+durationGap);
	ns3TcpSocket2->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, h2cw, 0));
	ns3TcpSocket2->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, h2cl, 0, 2));

	sink = "/NodeList/6/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sink, MakeBoundCallback(&recvpacket, h2gp, 0));
	sink_ = "/NodeList/6/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sink_, MakeBoundCallback(&recvpacketv4, h2tp, 0));

	//TCP Fack from H3 to H6
	std::cout << "** TCP Yeah from H3 to H6" << std::endl;
	Ptr<OutputStreamWrapper> h3cw = asciiTraceHelper.CreateFileStream("PartB/DataFiles/data_yeah_b.cw");
	Ptr<OutputStreamWrapper> h3cl = asciiTraceHelper.CreateFileStream("PartB/DataFiles/yeah.cl");
	Ptr<OutputStreamWrapper> h3tp = asciiTraceHelper.CreateFileStream("PartB/DataFiles/data_yeah_b.tp");
	Ptr<OutputStreamWrapper> h3gp = asciiTraceHelper.CreateFileStream("PartB/DataFiles/data_yeah_b.gp");
	Ptr<Socket> ns3TcpSocket3 = Simulate(InetSocketAddress(receiverIFCs.GetAddress(2), port), port, "TcpYeah", senders.Get(2), receivers.Get(2), otherFlowStart, otherFlowStart+durationGap, params.packetSize, numPackets, transferSpeed, otherFlowStart, otherFlowStart+durationGap);
	ns3TcpSocket3->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, h3cw, 0));
	ns3TcpSocket3->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, h3cl, 0, 3));

	sink = "/NodeList/7/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sink, MakeBoundCallback(&recvpacket, h3gp, 0));
	sink_ = "/NodeList/7/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sink_, MakeBoundCallback(&recvpacketv4, h3tp, 0));

	std::cout << "Populating Routing tables...";
	Ipv4GlobalRoutingHelper::PopulateRoutingTables();
	std::cout<<"done"<< std::endl;

	std::cout << "Setting up FlowMonitor...";
	Ptr<FlowMonitor> flowmon;
	FlowMonitorHelper flowmonHelper;
	flowmon = flowmonHelper.InstallAll();
	Simulator::Stop(Seconds(durationGap+otherFlowStart));
	std::cout<<"done"<< std::endl;

	std::cout<<"Starting Simulation..."<< std::endl;
	Simulator::Run();

	std::cout<<"Checking for lost packets..."<< std::endl;
	flowmon->CheckForLostPackets();

	//Ptr<OutputStreamWrapper> streamTP = asciiTraceHelper.CreateFileStream("application_6_b.tp");
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
	std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();
	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
		Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
		if(t.sourceAddress == "172.15.0.1") {
			if(mapDrop.find(1)==mapDrop.end())
				mapDrop[1] = 0;
			*h1cl->GetStream() << "TcpHybla Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
			*h1cl->GetStream()  << "Net Packet Lost: " << i->second.lostPackets << "\n";
			*h1cl->GetStream()  << "Packet Lost due to buffer overflow: " << mapDrop[1] << "\n";
			*h1cl->GetStream()  << "Packet Lost due to Congestion: " << i->second.lostPackets - mapDrop[1] << "\n";
			*h1cl->GetStream() << "Max throughput: " << mapMaxThroughput["/NodeList/5/$ns3::Ipv4L3Protocol/Rx"] << std::endl;
		} else if(t.sourceAddress == "172.15.1.1") {
			if(mapDrop.find(2)==mapDrop.end())
				mapDrop[2] = 0;
			*h2cl->GetStream() << "TcpWestwood Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
			*h2cl->GetStream()  << "Net Packet Lost: " << i->second.lostPackets << "\n";
			*h2cl->GetStream()  << "Packet Lost due to buffer overflow: " << mapDrop[2] << "\n";
			*h2cl->GetStream()  << "Packet Lost due to Congestion: " << i->second.lostPackets - mapDrop[2] << "\n";
			*h2cl->GetStream() << "Max throughput: " << mapMaxThroughput["/NodeList/6/$ns3::Ipv4L3Protocol/Rx"] << std::endl;
		} else if(t.sourceAddress == "172.15.2.1") {
			if(mapDrop.find(3)==mapDrop.end())
				mapDrop[3] = 0;
			*h3cl->GetStream() << "TcpYeah Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
			*h3cl->GetStream()  << "Net Packet Lost: " << i->second.lostPackets << "\n";
			*h3cl->GetStream()  << "Packet Lost due to buffer overflow: " << mapDrop[3] << "\n";
			*h3cl->GetStream()  << "Packet Lost due to Congestion: " << i->second.lostPackets - mapDrop[3] << "\n";
			*h3cl->GetStream() << "Max throughput: " << mapMaxThroughput["/NodeList/7/$ns3::Ipv4L3Protocol/Rx"] << std::endl;
		}
	}

	//flowmon->SerializeToXmlFile("application_6_b.flowmon", true, true);
	std::cout << "Simulation finished! Find the data in PartB folder" << std::endl;
	Simulator::Destroy();
	return 0;

}