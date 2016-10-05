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

// ndn-simple.cpp

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/lte-module.h"
#include "ns3/lte-helper.h"
#include <ns3/buildings-helper.h>
#include <ns3/config-store.h>
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/epc-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
//#include "ns3/internet-module.h"

#include <fstream>
#include <string>
/////////////////////////////////////////////////////////////////////////////
// Check what protocols are already implemented and discuss it with Eirini //
/////////////////////////////////////////////////////////////////////////////
// obviously there are some forward strategies already implemented but I
// don't know much about them. yet.
/////////////////////////////////////////////////////////////////////////////

namespace ns3 {

/**
 * This scenario simulates a very simple network topology:
 *
 *
 *      +----------+     1Mbps      +--------+     1Mbps      +----------+
 *      | consumer | <------------> | router | <------------> | producer |
 *      +----------+         10ms   +--------+          10ms  +----------+
 *
 *
 * Consumer requests data from producer with frequency 10 interests per second
 * (interests contain constantly increasing sequence number).
 *
 * For every received interest, producer replies with a data packet, containing
 * 1024 bytes of virtual payload.
 *
 * To run scenario and see what is happening, use the following command:
 *
 *     NS_LOG=ndn.Consumer:ndn.Producer ./waf --run=ndn-simple
 */

int
main(int argc, char* argv[])
{
	  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
	  CommandLine cmd;
	  cmd.Parse(argc, argv);
/*
  std::cout << "This is a Test and works regardless of the loglevel" << endl;
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("20"));

  // Find out where argv[] gets this debug value...
  std::cout << "This is argc: " << argc << std::endl;
  std::cout << "this is argv[]: " << *argv << std::endl;
*/



  // Creating nodes
  NodeContainer nodes;
  nodes.Create(4);

	  // MOBILITY HELPER START
	  MobilityHelper mobility;

	  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
			  "MinX", DoubleValue (0.0),
			  "MinY", DoubleValue (0.0),
			  "DeltaX", DoubleValue (50.0),
			  "DeltaY", DoubleValue (10.0),
			  "GridWidth", UintegerValue (300),
			  "LayoutType", StringValue ("RowFirst"));

	  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
			  "Bounds", RectangleValue (Rectangle (-500, 500, -500, 500)));

	  mobility.Install (nodes);
	  // MOBILITY HELPER END


  // disable fragmentation
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
                     StringValue("OfdmRate24Mbps"));




  //Ptr<>
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
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  BuildingsHelper::Install (nodes);
  ////////////////
  // 1. Install Wifi
  NetDeviceContainer wifiNetDevices = wifi.Install(wifiPhyHelper, wifiMacHelper, nodes);


  NetDeviceContainer enbDevs;
  NetDeviceContainer ueDevs;



 //





  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.InstallAll();

  ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/multicast");


  // Consumer
  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
  // Consumer will request /prefix/0, /prefix/1, ...
  consumerHelper.SetPrefix("/prefix");
  consumerHelper.SetAttribute("Frequency", StringValue("10")); // 10 interests a second
  consumerHelper.Install(nodes.Get(1));                        // first node

  // Producer
  ndn::AppHelper producerHelper("ns3::ndn::Producer");
  // Producer will reply to all requests starting with /prefix
  producerHelper.SetPrefix("/prefix");
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.Install(nodes.Get(3)); // last node
  //producerHelper.Install(nodes.Get(1));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
