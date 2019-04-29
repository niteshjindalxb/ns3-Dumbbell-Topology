#include "header.h"

int main() 
{
	std::cout << "* PART-1 AND PART-3 STARTED *" << std::endl;
	TopologyParam params;
	
	PointToPointHelper p2pHR = configureP2PHelper(params.rateHR, params.latencyHR, std::to_string(params.queueSizeHR)+"p");
	PointToPointHelper p2pRR = configureP2PHelper(params.rateRR, params.latencyRR, std::to_string(params.queueSizeRR)+"p");
	Ptr<RateErrorModel> errorModel = CreateObjectWithAttributes<RateErrorModel> ("ErrorRate", DoubleValue (params.errorParam));

	//Empty node containers
	NodeContainer routers, senders, receivers;
	//Create n nodes and append pointers to them to the end of this NodeContainer. 
	routers.Create(params.numRouters);
	senders.Create(params.numSender);
	receivers.Create(params.numRecv);

	NetDeviceContainer routerDevices = p2pRR.Install(routers);
	
	//Empty netdevicecontatiners
	NetDeviceContainer leftRouterDevices, rightRouterDevices, senderDevices, receiverDevices;

	//Adding links
	std::cout << "Adding links...";

	int i = 0;
	while(i< params.numSender)
	{
		NetDeviceContainer cleft = p2pHR.Install(routers.Get(0), senders.Get(i));
		leftRouterDevices.Add(cleft.Get(0));
		senderDevices.Add(cleft.Get(1));
		cleft.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(errorModel));
		i++;
	}

	i=0;
	while(i< params.numSender)
	{
		NetDeviceContainer cright = p2pHR.Install(routers.Get(1), receivers.Get(i));
		rightRouterDevices.Add(cright.Get(0));
		receiverDevices.Add(cright.Get(1));
		cright.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(errorModel));
		i++;
	}

	std::cout<<"done"<< std::endl;
	
	//Install Internet Stack
	/*
		For each node in the input container, aggregate implementations of 
		the ns3::Ipv4, ns3::Ipv6, ns3::Udp, and, ns3::Tcp classes. 
	*/
	std::cout << "Installing internet stack...";
	InternetStackHelper stack;
	stack.Install(routers);
	stack.Install(senders);
	stack.Install(receivers);
	std::cout<<"done"<< std::endl;

	//Adding IP addresses
	std::cout << "Assigning IP addresses...";
	Ipv4AddressHelper routerIP = Ipv4AddressHelper("10.3.0.0", "255.255.255.0");	//(network, mask)
	Ipv4AddressHelper senderIP = Ipv4AddressHelper("10.1.0.0", "255.255.255.0");
	Ipv4AddressHelper receiverIP = Ipv4AddressHelper("10.2.0.0", "255.255.255.0");

	Ipv4InterfaceContainer routerIFC, senderIFCs, receiverIFCs, leftRouterIFCs, rightRouterIFCs;

	//Assign IP addresses to the net devices specified in the container 
	//based on the current network prefix and address base
	routerIFC = routerIP.Assign(routerDevices);

	for(int i = 0; i < params.numSender; ++i) 
	{
		NetDeviceContainer senderDevice;
		senderDevice.Add(senderDevices.Get(i));
		senderDevice.Add(leftRouterDevices.Get(i));

		Ipv4InterfaceContainer senderIFC = senderIP.Assign(senderDevice);
		senderIFCs.Add(senderIFC.Get(0));
		leftRouterIFCs.Add(senderIFC.Get(1));
		//Increment the network number and reset the IP address counter 
		//to the base value provided in the SetBase method.
		senderIP.NewNetwork();

		NetDeviceContainer receiverDevice;
		receiverDevice.Add(receiverDevices.Get(i));
		receiverDevice.Add(rightRouterDevices.Get(i));
		
		Ipv4InterfaceContainer receiverIFC = receiverIP.Assign(receiverDevice);
		receiverIFCs.Add(receiverIFC.Get(0));
		rightRouterIFCs.Add(receiverIFC.Get(1));
		receiverIP.NewNetwork();
	}

	std::cout<<"done"<< std::endl;

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
	std::cout<<"** TCP Hybla from H1 to H4 **"<<std::endl;
	AsciiTraceHelper asciiTraceHelper;
	Ptr<OutputStreamWrapper> h1cw = asciiTraceHelper.CreateFileStream("PartA/data_hybla_a.cw");
	Ptr<OutputStreamWrapper> h1cl = asciiTraceHelper.CreateFileStream("PartA/hybla_a.cl");
	Ptr<OutputStreamWrapper> h1tp = asciiTraceHelper.CreateFileStream("PartA/data_hybla_a.tp");
	Ptr<OutputStreamWrapper> h1gp = asciiTraceHelper.CreateFileStream("PartA/data_hybla_a.gp");
	Ptr<Socket> ns3TcpSocket1 = Simulate(InetSocketAddress(receiverIFCs.GetAddress(0), port), port, "TcpHybla", senders.Get(0), receivers.Get(0), netDuration, netDuration+durationGap, params.packetSize, numPackets, transferSpeed, netDuration, netDuration+durationGap);
	ns3TcpSocket1->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, h1cw, netDuration));
	ns3TcpSocket1->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, h1cl, netDuration, 1));

	// Measure PacketSinks
	std::string sink = "/NodeList/5/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sink, MakeBoundCallback(&ReceivedPacket, h1gp, netDuration));

	std::string sink_ = "/NodeList/5/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sink_, MakeBoundCallback(&ReceivedPacketIPV4, h1tp, netDuration));

	netDuration += durationGap;


	//TCP Westwood from H2 to H5
	std::cout<<"** TCP Westwood from H2 to H5 **"<<std::endl;
	Ptr<OutputStreamWrapper> h2cw = asciiTraceHelper.CreateFileStream("PartA/data_westwood_a.cw");
	Ptr<OutputStreamWrapper> h2cl = asciiTraceHelper.CreateFileStream("PartA/westwood_a.cl");
	Ptr<OutputStreamWrapper> h2tp = asciiTraceHelper.CreateFileStream("PartA/data_westwood_a.tp");
	Ptr<OutputStreamWrapper> h2gp = asciiTraceHelper.CreateFileStream("PartA/data_westwood_a.gp");
	Ptr<Socket> ns3TcpSocket2 = Simulate(InetSocketAddress(receiverIFCs.GetAddress(1), port), port, "TcpWestwood", senders.Get(1), receivers.Get(1), netDuration, netDuration+durationGap, params.packetSize, numPackets, transferSpeed, netDuration, netDuration+durationGap);
	ns3TcpSocket2->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, h2cw, netDuration));
	ns3TcpSocket2->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, h2cl, netDuration, 2));

	sink = "/NodeList/6/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sink, MakeBoundCallback(&ReceivedPacket, h2gp, netDuration));
	sink_ = "/NodeList/6/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sink_, MakeBoundCallback(&ReceivedPacketIPV4, h2tp, netDuration));
	netDuration += durationGap;

	//TCP Fack from H3 to H6
	std::cout<<"** TCP Yeah from H3 to H6 **"<<std::endl;
	Ptr<OutputStreamWrapper> h3cw = asciiTraceHelper.CreateFileStream("PartA/data_yeah_a.cw");
	Ptr<OutputStreamWrapper> h3cl = asciiTraceHelper.CreateFileStream("PartA/yeah_a.cl");
	Ptr<OutputStreamWrapper> h3tp = asciiTraceHelper.CreateFileStream("PartA/data_yeah_a.tp");
	Ptr<OutputStreamWrapper> h3gp = asciiTraceHelper.CreateFileStream("PartA/data_yeah_a.gp");
	Ptr<Socket> ns3TcpSocket3 = Simulate(InetSocketAddress(receiverIFCs.GetAddress(2), port), port, "TcpYeah", senders.Get(2), receivers.Get(2), netDuration, netDuration+durationGap, params.packetSize, numPackets, transferSpeed, netDuration, netDuration+durationGap);
	ns3TcpSocket3->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback (&CwndChange, h3cw, netDuration));
	ns3TcpSocket3->TraceConnectWithoutContext("Drop", MakeBoundCallback (&packetDrop, h3cl, netDuration, 3));

	sink = "/NodeList/7/ApplicationList/0/$ns3::PacketSink/Rx";
	Config::Connect(sink, MakeBoundCallback(&ReceivedPacket, h3gp, netDuration));
	sink_ = "/NodeList/7/$ns3::Ipv4L3Protocol/Rx";
	Config::Connect(sink_, MakeBoundCallback(&ReceivedPacketIPV4, h3tp, netDuration));
	netDuration += durationGap;

	//p2pHR.EnablePcapAll("application_6__a");
	//p2pRR.EnablePcapAll("application_6_RR_a");

	//Turning on Static Global Routing
	std::cout<<"Populating Routing Tables...";
	Ipv4GlobalRoutingHelper::PopulateRoutingTables();
	std::cout<<"done"<< std::endl;

	std::cout<<"Setting up FlowMonitor...";
	Ptr<FlowMonitor> flowmon;
	FlowMonitorHelper flowmonHelper;
	flowmon = flowmonHelper.InstallAll();
	Simulator::Stop(Seconds(netDuration));
	std::cout<<"done"<< std::endl;

	std::cout<<"Starting Simulation"<< std::endl;

	Simulator::Run();
	std::cout<<"Checking for lost packets...";
	flowmon->CheckForLostPackets();
	std::cout<<"done"<< std::endl;

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
			*h1cl->GetStream() << "TcpHybla Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
			*h1cl->GetStream()  << "Net Packet Lost: " << i->second.lostPackets << "\n";
			*h1cl->GetStream()  << "Packet Lost due to buffer overflow: " << mapDrop[1] << "\n";
			*h1cl->GetStream()  << "Packet Lost due to Congestion: " << i->second.lostPackets - mapDrop[1] << "\n";
			*h1cl->GetStream() << "Max throughput: " << mapMaxThroughput["/NodeList/5/$ns3::Ipv4L3Protocol/Rx"] << std::endl;
		} else if(t.sourceAddress == "10.1.1.1") {
			if(mapDrop.find(2)==mapDrop.end())
				mapDrop[2] = 0;
			*h2cl->GetStream() << "Tcp Westwood Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
			*h2cl->GetStream()  << "Net Packet Lost: " << i->second.lostPackets << "\n";
			*h2cl->GetStream()  << "Packet Lost due to buffer overflow: " << mapDrop[2] << "\n";
			*h2cl->GetStream()  << "Packet Lost due to Congestion: " << i->second.lostPackets - mapDrop[2] << "\n";
			*h2cl->GetStream() << "Max throughput: " << mapMaxThroughput["/NodeList/6/$ns3::Ipv4L3Protocol/Rx"] << std::endl;
		} else if(t.sourceAddress == "10.1.2.1") {
			if(mapDrop.find(3)==mapDrop.end())
				mapDrop[3] = 0;
			*h3cl->GetStream() << "Tcp Fack Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
			*h3cl->GetStream()  << "Net Packet Lost: " << i->second.lostPackets << "\n";
			*h3cl->GetStream()  << "Packet Lost due to buffer overflow: " << mapDrop[3] << "\n";
			*h3cl->GetStream()  << "Packet Lost due to Congestion: " << i->second.lostPackets - mapDrop[3] << "\n";
			*h3cl->GetStream() << "Max throughput: " << mapMaxThroughput["/NodeList/7/$ns3::Ipv4L3Protocol/Rx"] << std::endl;
		}
	}

	//flowmon->SerializeToXmlFile("application_6_a.flowmon", true, true);
	std::cout << "Simulation finished! Find the data in PartA folder" << std::endl;
	Simulator::Destroy();
	return 0;
}