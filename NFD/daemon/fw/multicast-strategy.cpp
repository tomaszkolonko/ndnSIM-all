/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2015,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "multicast-strategy.hpp"
#include "ns3/node.h"
#include "ns3/node-list.h"
#include <typeinfo>

namespace nfd {
namespace fw {

const Name MulticastStrategy::STRATEGY_NAME("ndn:/localhost/nfd/strategy/multicast/%FD%01");
NFD_REGISTER_STRATEGY(MulticastStrategy);

const bool debug = false;
static int isBeingForwardedTo = 0;
static int isNotBeingForwardedTo = 0;

static const int NET_DEVICE_ZERO = 0;
static const int NET_DEVICE_ONE = 1;
static const int NET_DEVICE_NONE = 2;
static const int NET_DEVICE_INVALID = -1;

static int semaphoreNetDevice = NET_DEVICE_INVALID;

static int netDeviceSemaphore = 0;

MulticastStrategy::MulticastStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder, name)
{
}

bool sortByLowestCost(fib::NextHop a, fib::NextHop b) { return a.getCost() < b.getCost(); }

void
MulticastStrategy::afterReceiveInterest(const Face& inFace,
                   const Interest& interest,
                   shared_ptr<fib::Entry> fibEntry,
                   shared_ptr<pit::Entry> pitEntry)
{
	bool NextHopHasValidMacAddress = false;
	// they are already sorted by cost !!!
	const fib::NextHopList& nexthops = fibEntry->getNextHops();
	std::vector<fib::NextHop> nextHopsSorted = fibEntry->getNextHops();

	std::sort(nextHopsSorted.begin(), nextHopsSorted.end(), sortByLowestCost);

//		// SORTING DOES WORK
//	  	for(int i = 0; i < nextHopsSorted.size(); i++) {
//	  		std::cout << " sorted ? " << nextHopsSorted[i].getCost() << std::endl;
//	  	}

	ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());

	std::string newCurrentOriginMac;

	int numberOfNetDevices = node->GetNDevices();
	std::ostringstream addr[numberOfNetDevices];
	std::string currentMacAddresses[numberOfNetDevices];
	for(int index = 0; index < numberOfNetDevices; index++) {
		addr[index] << node->GetDevice(index)->GetAddress();
		currentMacAddresses[index] = addr[index].str().substr(6);
	}

	// Check on which netDevice the interest arrived and save it
	if(std::regex_match(interest.getInterestTargetMacAddress(), std::regex("([0-9a-fA-F]{2}[:-]){5}[0-9a-fA-F]{2}"))) {
		if(currentMacAddresses[0] == interest.getInterestTargetMacAddress()) {
	      semaphoreNetDevice = NET_DEVICE_ZERO;
		  //found = interest_macAddressPath.find(currentMacAddresses[semaphoreNetDevice]);
		} else if(currentMacAddresses[1] == interest.getInterestTargetMacAddress()) {
		  semaphoreNetDevice = NET_DEVICE_ONE;
		  //found = interest_macAddressPath.find(currentMacAddresses[semaphoreNetDevice]);
		} else {
		  semaphoreNetDevice = NET_DEVICE_INVALID;
		  // DROP THE INTEREST SINCE IT HAS A MAC THAT WAS NOT MEANT FOR THIS NODE / NETDEVICE
		  // Should have been done before already....
		  std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
		  std::cout << "                       dropping interests does not work                             " << std::endl;
		  std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
		  return;
		}
	} else {
		// BROADCAST THE INTEREST
		semaphoreNetDevice = NET_DEVICE_NONE;
	}

	if(semaphoreNetDevice == NET_DEVICE_NONE) {
		newCurrentOriginMac = currentMacAddresses[netDeviceSemaphore%2];
		netDeviceSemaphore++;
	} else if(semaphoreNetDevice == NET_DEVICE_ZERO || semaphoreNetDevice == NET_DEVICE_ONE){
		newCurrentOriginMac = currentMacAddresses[semaphoreNetDevice];
	} else {
		std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
		std::cout << "                            mustn't be possible                                     " << std::endl;
		std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
		newCurrentOriginMac = "undefined";
	}


												// SOME PRINT STATEMENTS IF NEEDED: BEGINN
												if(false) printPITInRecord(pitEntry, node);
												if(false) printPITOutRecord(pitEntry);
												if(node->GetId() == 0){
													if(false) printPITOriginMacRecord(pitEntry);
												}
												if(false) printFIBTargetMacRecord(fibEntry);
												// SOME PRINT STATEMENTS IF NEEDED: END


	for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it) {
//		 std::cout << "4765: YOU ARE ON NODE (" << node->GetId() << ")" << " FIB ENTRY NAME: " << fibEntry->getPrefix()
//				 << " size of next hops: " << nexthops.size() << " with target mac ad: " << it->getTargetMac()
//				 << "-- COST: " << it->getCost() << std::endl;

		if (std::regex_match(it->getTargetMac(), std::regex("([0-9a-fA-F]{2}[:-]){5}[0-9a-fA-F]{2}"))){
			std::cout << "it->getTargetMac() " << it->getTargetMac()  << " and cost: " << it->getCost() << std::endl;
			NextHopHasValidMacAddress = true;
		} else {
			std::cout << "it->getTargetMac() " << it->getTargetMac()  << " and cost: " << it->getCost() << std::endl;
		}
	}

	if (NextHopHasValidMacAddress == true) { // SEND OUT SPECIFICALLY BRANCH
		int i = 0;
		int forwarded = 0;
		//for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it, ++i) {

		for(int vectorIndex = 0; vectorIndex < nextHopsSorted.size(); vectorIndex++) {

			// send only to the best three nextHops
			if(i >= 1) break;

		  shared_ptr<Face> outFace = nextHopsSorted[vectorIndex].getFace();

		  std::string targetMac; // = it->getMac();

		  // if the targetMac of the interest is the same as of the targetMac of the NextHop, the interest will loop.
		  if(interest.getInterestTargetMacAddress() == nextHopsSorted[vectorIndex].getTargetMac()) {
			  i--;
//			  std::cout << "\n&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
//			  std::cout << "                           looping interests detected                               " << std::endl;
//			  std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
			  continue;
			  // this->rejectPendingInterest(pitEntry);
		  }

		  if(std::regex_match(nextHopsSorted[vectorIndex].getTargetMac(), std::regex("([0-9a-fA-F]{2}[:-]){5}[0-9a-fA-F]{2}"))) {
			  nextHopsSorted[vectorIndex].incrementCost();
			  std::cout << "it->getCost(): " << nextHopsSorted[vectorIndex].getCost() << std::endl;
			  targetMac = nextHopsSorted[vectorIndex].getTargetMac();
		  } else {
			  i--;
//			  std::cout << "\n&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
//			  std::cout << "                             no targetMac on NextHop                                " << std::endl;
//			  std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
			  continue;
		  }


		   // if (pitEntry->canForwardTo(*outFace)) {
		  if (true) {
			  isBeingForwardedTo++;
			  forwarded++;
			  std::cout << "\n&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
			  std::cout << "                                YEAH forward to: " << targetMac << "                         " << std::endl;
			  std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
			this->sendInterest(pitEntry, outFace, newCurrentOriginMac, targetMac, inFace.getId());
		  } else {
			  // find a new nexthop!
			  i--;
			  std::cout << "\n&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
			  std::cout << "                                cannot forward to: " << targetMac << "                         " << std::endl;
			  std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
			  isNotBeingForwardedTo++;
			}
		}
//		if(forwarded <= 1) {
//			  std::cout << "\n&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
//			  std::cout << "                                being broadcasted after all -> " << forwarded << "                        " << std::endl;
//			  std::cout << "&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&" << std::endl;
//			// if(pitEntry->canForwardTo(*outFace) was false for all NextHops with a target mac, BROADCAST INTEREST !!!
//			NextHopHasValidMacAddress = false;
//		}
	}

	if (NextHopHasValidMacAddress == false) { // BROADCAST BRANCH
		std::string originMacBroadcasting;
		unsigned int semaphoreBraodcasting = 0;
		int twice = 0;
		for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it) {
			shared_ptr<Face> outFace = it->getFace();
			std::string targetMac = "";
			if (pitEntry->canForwardTo(*outFace) && twice <= 2) {
				twice++;
				originMacBroadcasting = currentMacAddresses[semaphoreBraodcasting%2];
				semaphoreBraodcasting++;
				this->sendInterest(pitEntry, outFace, originMacBroadcasting, targetMac, inFace.getId());
			}
		}
	}

//	if (NextHopHasValidMacAddress == false) { // BROADCAST BRANCH
//		std::string originMacBroadcasting;
//		int smally = 0;
//
//		for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it, smally++) {
//			if(smally >= 2) break;
//			std::cout << "broadcasting : " << smally << " through: " << currentMacAddresses[smally] << std::endl;
//			originMacBroadcasting = currentMacAddresses[smally];
//
//			this->sendInterest(pitEntry, it->getFace(), originMacBroadcasting, "", inFace.getId());
//
//		}
//	}


	if (!pitEntry->hasUnexpiredOutRecords()) {
	  this->rejectPendingInterest(pitEntry);
	}
}

// iterates through all inRecords of ONE pitEntry
// Check FaceRecord for more options to display
void
MulticastStrategy::printPITInRecord(shared_ptr<pit::Entry> pitEntry, ns3::Ptr<ns3::Node> node) {
	const pit::InRecordCollection& inRecCol = pitEntry->getInRecords();
	std::cout << std::endl;
	std::cout << "MulticastStrategy::printPITInRecord" << std::endl;
	for(pit::InRecordCollection::const_iterator inIt = inRecCol.begin(); inIt != inRecCol.end(); ++inIt){
		std::cout << "inRecord interest: " << inIt->getInterest().getName() << std::endl;
		std::cout << "inRecord face: " << inIt->getFace()->getId() << std::endl;
		std::cout << "pointer to pitEntry: " << pitEntry << " on node: " << node->GetId() << std::endl;
	}
	std::cout << std::endl;
}

// iterates through all outRecords of ONE pitEntry
// Check FaceRecord for more options to display
void
MulticastStrategy::printPITOutRecord(shared_ptr<pit::Entry> pitEntry) {
	const pit::OutRecordCollection& outRecCol = pitEntry->getOutRecords();
	std::cout << std::endl;
	std::cout << "MulticastStrategy::printPITOutRecord" << std::endl;
	std::cout << "pointer to pitEntry: " << pitEntry << std::endl;
	std::cout << "outRecordCollection.size() " << outRecCol.size() << std::endl;
	for(pit::OutRecordCollection::const_iterator inIt = outRecCol.begin(); inIt != outRecCol.end(); ++inIt){
		std::cout << "outRecord face: " << inIt->getFace()->getId() << std::endl;
	}
	std::cout << std::endl;
}

void
MulticastStrategy::printPITOriginMacRecord(shared_ptr<pit::Entry> pitEntry) {
	const pit::OriginMacCollection& originMacCol = pitEntry->getOriginMacRecords();
	std::cout << std::endl;
	std::cout << "Multicaststrategy::printPITMacRecord" << std::endl;
	std::cout << "pointer to pitEntry: " << pitEntry << std::endl;
	std::cout << "originMacCollection.size() " << originMacCol.size() << std::endl;
	for(pit::OriginMacCollection::const_iterator macIt = originMacCol.begin(); macIt != originMacCol.end(); macIt++) {
		std::cout << "saved origin Mac is: " << *macIt << std::endl;
	}
	std::cout << "qwertz" << std::endl;
}

void
MulticastStrategy::printFIBTargetMacRecord(shared_ptr<fib::Entry> fibEntry) {
	std::vector<fib::NextHop> NextHopList = fibEntry->getNextHops();
	std::cout << std::endl;
	std::cout << "586" << std::endl;
	std::cout << "Multicaststrategy::printFIBTargetMacRecord" << std::endl;
	std::cout << "pointer to fibEntry: " << fibEntry << std::endl;
	std::cout << "fib::NextHop vector size: " << NextHopList.size() << std::endl;

	int index = 1;
	for(std::vector<fib::NextHop>::iterator it = NextHopList.begin(); it != NextHopList.end(); ++it, index++) {
		std::cout << index << ") it->getTargetMac(): " << it->getTargetMac() << " cost: " << it->getCost() << std::endl;
	}
}

} // namespace fw
} // namespace nfd
