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

NS_LOG_COMPONENT_DEFINE ("DumbbellTopology");

class DumbbellTopology: public Application {
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
		DumbbellTopology();
		virtual ~DumbbellTopology() 	{		mSocket = 0;		}

		void Setup(Ptr<Socket> socket, Address address, int packetSize, int nPackets, DataRate dataRate);
		void ChangeRate(DataRate newRate);
		void recv(int numBytesRcvd);

};

extern std::map<int, int> mapDrop;
extern std::map<Address, double> mapBytesReceived;
extern std::map<std::string, double> mapBytesReceivedIPV4, mapMaxThroughput;

extern static double lastTimePrint = 0;
extern static double lastTimePrintIPV4 = 0;
extern double printGap = 0;

struct TopologyParam
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

	TopologyParam();
};

PointToPointHelper configureP2PHelper(std::string rate, std::string latency, std::string s);
static void CwndChange(Ptr<OutputStreamWrapper> stream, double startTime, int oldCwnd, int newCwnd);
static void packetDrop(Ptr<OutputStreamWrapper> stream, double startTime, int myId);
void IncRate(Ptr<DumbbellTopology> app, DataRate rate);
void ReceivedPacket(Ptr<OutputStreamWrapper> stream, double startTime, std::string context, Ptr<const Packet> p, const Address& addr);
void ReceivedPacketIPV4(Ptr<OutputStreamWrapper> stream, double startTime, std::string context, Ptr<const Packet> p, Ptr<Ipv4> ipv4, int interface);
Ptr<Socket> Simulate(Address sinkAddress, 
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
					double appStopTime);


#endif