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

#ifndef HEADER_H
#define HEADER_H

typedef uint32_t int;

using namespace ns3;

#define ERROR 0.000001

NS_LOG_COMPONENT_DEFINE ("TestApp");

class TestApp: public Application {
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
		TestApp();
		virtual ~TestApp() 	{		mSocket = 0;		}

		void Setup(Ptr<Socket> socket, Address address, int packetSize, int nPackets, DataRate dataRate);
		void ChangeRate(DataRate newRate);
		void recv(int numBytesRcvd);

};

TestApp::TestApp(): mSocket(0),
		    mPeer(),
		    mPacketSize(0),
		    mNPackets(0),
		    mDataRate(0),
		    mSendEvent(),
		    mRunning(false),
		    mPacketsSent(0) {
}

TestApp::~TestApp() {
	mSocket = 0;
}

void TestApp::Setup(Ptr<Socket> socket, Address address, uint packetSize, uint nPackets, DataRate dataRate) {
	mSocket = socket;
	mPeer = address;
	mPacketSize = packetSize;
	mNPackets = nPackets;
	mDataRate = dataRate;
}

void TestApp::StartApplication() {
	mRunning = true;
	mPacketsSent = 0;
	mSocket->Bind();
	mSocket->Connect(mPeer);
	SendPacket();
}

void TestApp::StopApplication() {
	mRunning = false;
	if(mSendEvent.IsRunning()) {
		Simulator::Cancel(mSendEvent);
	}
	if(mSocket) {
		mSocket->Close();
	}
}

void TestApp::SendPacket() {
	Ptr<Packet> packet = Create<Packet>(mPacketSize);
	mSocket->Send(packet);

	if(++mPacketsSent < mNPackets) {
		ScheduleTx();
	}
}

void TestApp::ScheduleTx() {
	if (mRunning) {
		Time tNext(Seconds(mPacketSize*8/static_cast<double>(mDataRate.GetBitRate())));
		mSendEvent = Simulator::Schedule(tNext, &TestApp::SendPacket, this);
		//double tVal = Simulator::Now().GetSeconds();
		//if(tVal-int(tVal) >= 0.99)
		//	std::cout << Simulator::Now ().GetSeconds () << "\t" << mPacketsSent << std::endl;
	}
}

void TestApp::ChangeRate(DataRate newrate) {
	mDataRate = newrate;
	return;
}

PointToPointHelper configureP2PHelper(std::string rate, std::string latency, std::string s)
{
	PointToPointHelper p2p;
	p2p.SetDeviceAttribute("DataRate", StringValue(rate));
	p2p.SetChannelAttribute("Delay", StringValue(latency));
	std::cout<<"Queue Size: "<<s<<std::endl;
	p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue(s));
	return p2p;
}

static void CwndChange(Ptr<OutputStreamWrapper> file, double startTime, uint oldCwnd, uint newCwnd) {
	*file->GetStream() << Simulator::Now ().GetSeconds () - startTime << "\t" << newCwnd << std::endl;
}

std::map<uint, uint> mapDrop;

static void packetDrop(Ptr<OutputStreamWrapper> file, double startTime, uint packetId) 
{
	*file->GetStream() << Simulator::Now ().GetSeconds () - startTime << "\t" << std::endl;
	
	if(mapDrop.find(packetId) == mapDrop.end()) {
		mapDrop[packetId] = 0;
	}
	mapDrop[packetId]++;
}

std::map<Address, double> recvbytes;
std::map<std::string, double> ipv4bytes, mapMaxThroughput;

static double lastTimePrint = 0, lastTimePrintIPV4 = 0;
double printGap = 0;

void recvpacket(Ptr<OutputStreamWrapper> file, double startTime, std::string context, 
Ptr<const Packet> p, const Address& addr)
{
	double timeNow = Simulator::Now().GetSeconds();

	if(recvbytes.find(addr) == recvbytes.end())
		recvbytes[addr] = 0;

	recvbytes[addr] += p->GetSize();

	double kbps_ = (((recvbytes[addr] * 8.0)/1024)/(timeNow-startTime));

	if(timeNow - lastTimePrint >= printGap) 
	{
		lastTimePrint = timeNow;
		*file->GetStream() << timeNow-startTime << "\t" <<  kbps_ << std::endl;
	}
}

void recvpacketv4(Ptr<OutputStreamWrapper> file, double startTime, 
std::string context, Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint interface) 
{
	double timeNow = Simulator::Now().GetSeconds();
	
	if(ipv4bytes.find(context) == ipv4bytes.end())
		ipv4bytes[context] = 0;
	
	if(mapMaxThroughput.find(context) == mapMaxThroughput.end())
		mapMaxThroughput[context] = 0;
	
	ipv4bytes[context] += p->GetSize();
	
	double kbps_ = (((ipv4bytes[context] * 8.0) / 1024)/(timeNow-startTime));
	
	if(timeNow - lastTimePrintIPV4 >= printGap)
	{
		lastTimePrintIPV4 = timeNow;
		*file->GetStream() << timeNow-startTime << "\t" <<  kbps_ << std::endl;
		
		if(mapMaxThroughput[context] < kbps_)
		{
			mapMaxThroughput[context] = kbps_;
		}
	}
}


Ptr<Socket> Simulate(Address sinkAddress, uint sinkPort, std::string type, Ptr<Node> hostNode, 
Ptr<Node> sinkNode, double startTime, double stopTime,uint packetSize,uint numPackets,
std::string dataRate,double appStartTime,double appStopTime) 
{
	if(type == "TcpHybla") 
	{
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpHybla::GetTypeId()));
	} 
	else if(type == "TcpWestwoodPlus") 
	{
		Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
	} 
	else if(type == "TcpYeah") 
	{
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpYeah::GetTypeId()));
	} 
	else 
	{
		fprintf(stderr, "Invalid TCP version\n");
		exit(EXIT_FAILURE);
	}

	PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
	ApplicationContainer sinkApps = packetSinkHelper.Install(sinkNode);
	sinkApps.Start(Seconds(startTime));
	sinkApps.Stop(Seconds(stopTime));

	Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(hostNode, TcpSocketFactory::GetTypeId());
	

	Ptr<TestApp> app = CreateObject<TestApp>();
	app->Setup(ns3TcpSocket, sinkAddress, packetSize, numPackets, DataRate(dataRate));
	hostNode->AddApplication(app);
	app->SetStartTime(Seconds(appStartTime));
	app->SetStopTime(Seconds(appStopTime));

	return ns3TcpSocket;
}

struct NetworkParam
{
	std::string rateHR;
	std::string latencyHR;
	std::string rateRR;
	std::string latencyRR;

	int packetSize;
	int queueSizeHR;
	int queueSizeRR;

	int numSender;
	int numRecv;
	int numRouters;

	double errorParam;

	NetworkParam();
};

PointToPointHelper configureP2PHelper(std::string rate, std::string latency, std::string s);
static void CwndChange(Ptr<OutputStreamWrapper> stream, double startTime, int oldCwnd, int newCwnd);
static void packetDrop(Ptr<OutputStreamWrapper> stream, double startTime, int myId);
void IncRate(Ptr<TestApp> app, DataRate rate);
void ReceivedPacket(Ptr<OutputStreamWrapper> stream, double startTime, std::string context, Ptr<const Packet> p, const Address& addr);
void ReceivedPacketIPV4(Ptr<OutputStreamWrapper> stream, double startTime, std::string context, Ptr<const Packet> p, Ptr<Ipv4> ipv4, int interface);
Ptr<Socket> Simulate(Address sinkAddress, int sinkPort, std::string tcpVariant, Ptr<Node> hostNode, Ptr<Node> sinkNode, double startTime, double stopTime, int packetSize, int numPackets, std::string dataRate, double appStartTime, double appStopTime);

#endif