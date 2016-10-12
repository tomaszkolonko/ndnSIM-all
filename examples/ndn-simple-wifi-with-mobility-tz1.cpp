/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ndnSIM/utils/tracers/ndn-l3-rate-tracer.hpp"

#include "ns3/ndnSIM-module.h"
//#include <ns3/ndnSIM/utils/tracers/ndn-cs-tracer.hpp>

using namespace std;
namespace ns3 {

NS_LOG_COMPONENT_DEFINE("ndn.WifiExample");

const bool debug = true;

void printNodes(NodeContainer nodes, int nodeNum)
{
  std::cout << "**********************************************" << std::endl;
  for(int i = 0; i < nodeNum; i++)
  {
    std::cout << "node(" << i << ") has " << (*nodes.Get(i)).GetNDevices() << " faces." << std::endl;
  }
  std::cout << "**********************************************" << std::endl;

  std::cout << "**********************************************" << std::endl;
  for(int i = 0; i < nodeNum; i++)
  {
    std::cout << "node: " << (*nodes.Get(i)).GetId() << " with faces: ";
    for(uint32_t n = 0; n < (*nodes.Get(i)).GetNDevices(); n++)
    {
      std::cout << (*nodes.Get(i)->GetDevice(n)).GetAddress () << "  ";
    }
    std::cout << std::endl;
  }
  std::cout << "**********************************************" << std::endl;
}

int
main(int argc, char* argv[])
{

  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  // LogComponentEnable("ndn.Consumer", LOG_LEVEL_DEBUG);
  // LogComponentEnable("ndn.Producer", LOG_LEVEL_DEBUG);

  // disable fragmentation
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
                     StringValue("OfdmRate24Mbps"));

  std::cout.precision (2);
  std::cout.setf (std::ios::fixed);
  // if this number is changed, you will need to update the consumerHelper and producerHelper-install methods
  int nodeNum = 6;

  double deltaTime = 10;
  std::string traceFile1 = "src/ndnSIM/examples/trace-files/ndn-simple-wifi-tracefile1";
  std::string traceFile2 = "src/ndnSIM/examples/trace-files/ndn-simple-wifi-tracefile2";
  std::string traceFile3 = "src/ndnSIM/examples/trace-files/ndn-simple-wifi-tracefile3";

  std::string currentTraceFile = traceFile3;

  CommandLine cmd;
  cmd.AddValue ("traceFile", "Ns2 movement trace file", currentTraceFile);
  cmd.AddValue ("deltaTime", "time interval (s) between updates (default 100)", deltaTime);
  // apparently that must be provided with same number of nodes as in tracefile
  cmd.AddValue ("nodeNum", "Number of nodes", nodeNum);
  cmd.Parse(argc, argv);

  //////////////////////
  WifiHelper wifi = WifiHelper::Default();
  // wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
                               StringValue("OfdmRate24Mbps"));

  YansWifiChannelHelper wifiChannel; // = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::ThreeLogDistancePropagationLossModel");
  wifiChannel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");

  // YansWifiPhy wifiPhy = YansWifiPhy::Default();
  YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default();
  wifiPhyHelper.SetChannel(wifiChannel.Create());
  wifiPhyHelper.Set("TxPowerStart", DoubleValue(5));
  wifiPhyHelper.Set("TxPowerEnd", DoubleValue(5));

  NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default();
  wifiMacHelper.SetType("ns3::AdhocWifiMac");

  NodeContainer nodes;
  Ptr<Node> node[nodeNum];

  // Nodes are being created as in variable nodeNum defines and added to an array of pointers
  // and to a nodeContainer for further processing.
  for(int i = 0; i < nodeNum; i++) {
	  node[i] = CreateObject<Node> ();
	  nodes.Add(node[i]);
  }

  if(debug) printNodes(nodes, nodeNum);

  // 1. Install Wifi
  NetDeviceContainer wifiNetDevices = wifi.Install(wifiPhyHelper, wifiMacHelper, nodes);
  NetDeviceContainer wifiNetDevices2 = wifi.Install(wifiPhyHelper, wifiMacHelper, nodes);

  // 2. Install Mobility model
  Ns2MobilityHelper ns2 = Ns2MobilityHelper (currentTraceFile);
  ns2.Install ();

  // 3. Install NDN stack -> L3Protocol
  NS_LOG_INFO("Installing NDN stack");
  if(debug) std::cout << "INSIDE MAIN: About to install NDN stack" << std::endl;
  ndn::StackHelper ndnHelper;
  // ndnHelper.AddNetDeviceFaceCreateCallback (WifiNetDevice::GetTypeId (), MakeCallback
  // (MyNetDeviceFaceCallback));
  ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", "1000");
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.Install(nodes);

  // AddRoute(Ptr<Node> node, const Name& prefix, uint32_t faceId, int32_t metric);

  std::string mac[(2*nodeNum)];
  Address ad;
  Address ad2;

  for(int n = 0; n < nodeNum; n++) {
	  ad = node[n]->GetDevice(0)->GetAddress();
	  std::ostringstream str;
	  str << ad;
	  mac[n] = str.str().substr(6);

	  ad2 = node[n]->GetDevice(1)->GetAddress();
	  std::ostringstream str2;
	  str2 << ad2;
	  mac[n+nodeNum] = str2.str().substr(6);
  }

  std::cout << std::endl << "**********************************************" << std::endl;
  std::cout << "testing mac0: " << mac[0] << std::endl;
  std::cout << "testing mac1: " << mac[1] << std::endl;
  std::cout << "testing mac2: " << mac[2] << std::endl;
  std::cout << "testing mac3: " << mac[3] << std::endl;
  std::cout << "testing mac4: " << mac[4] << std::endl;
  std::cout << "**********************************************" << std::endl << std::endl;


  //  ndn::FibHelper::AddRoute(node[0], "/", 256, 555, mac[1]); // mac[1] = 00:00:00:00:00:02 Node[1]
  //  ndn::FibHelper::AddRoute(node[2], "/", 256, 555, mac[4]); // mac[2] = 00:00:00:00:00:03 Node[2]

  // Set BestRoute strategy
  ndn::StrategyChoiceHelper::Install(nodes, "/", "/localhost/nfd/strategy/multicast");

  // 4. Set up applications
  NS_LOG_INFO("Installing Applications");

  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  consumerHelper.SetPrefix("/test/prefix");
  consumerHelper.SetAttribute("Frequency", DoubleValue(10.0));
  ApplicationContainer consumer = consumerHelper.Install(nodes.Get(0));
  consumer.Start(Seconds(2));
  consumer.Stop(Seconds(10));

  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  producerHelper.SetPrefix("/");
  producerHelper.SetAttribute("PayloadSize", StringValue("1200"));
  producerHelper.Install(nodes.Get(5));

  if(debug) printNodes(nodes, nodeNum);


  ndn::L3RateTracer::InstallAll("rate-trace.txt", Seconds(1));

  //  Does not work ;(
  //  ndn::CsTracer::Install(nodes, "cs-trace.txt", Seconds(1));
  //  ndn::CsTracer::InstallALL("cs-trace.txt", Seconds(1));

  Simulator::Stop(Seconds(10));

  Simulator::Run();
  /*
  if(debug)
  {
	  shared_ptr<fib> fib = forwarder.getFib();
	  std::cout << "contents of FIB: " << fib.size();
  }
  */

  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
