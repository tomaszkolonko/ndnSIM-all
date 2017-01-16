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
static int yes = 0;
static int no = 0;
static int shouldNotHappnen = 0;

MulticastStrategy::MulticastStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder, name)
{
}

void
MulticastStrategy::afterReceiveInterest(const Face& inFace,
                   const Interest& interest,
                   shared_ptr<fib::Entry> fibEntry,
                   shared_ptr<pit::Entry> pitEntry)
{
	// they are already sorted by cost !!!
	const fib::NextHopList& nexthops = fibEntry->getNextHops();
	ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
	std::cout << "you are on node (" << node->GetId() << ") " << std::endl;

	if(false) printPITInRecord(pitEntry, node);
	if(false) printPITOutRecord(pitEntry);
	if(true) printPITOriginMacRecord(pitEntry);
	if(true) printFIBTargetMacRecord(fibEntry);

	ns3::Address ad;
	std::ostringstream addr;
	addr << node->GetDevice(0)->GetAddress();
	std::string currentMacAddress = addr.str().substr(6);

	int i = 0;
	//std::cout << "ttt int i = " << i << std::endl;
	for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it, ++i) {

		// send only to the best three nextHops
		if(i >= 3) break;

	  shared_ptr<Face> outFace = it->getFace();

	  std::string targetMac; // = it->getMac();

	  // if FIB entry has been already updated with MacAddress then take it for later attachement to
	  // the interest. Otherwise an empty string will be added to the interest in the forwarder.

	  std::cout << "4765: YOU ARE ON NODE (" << node->GetId() << ")" << std::endl;

	  // TODO: sometimes current mac and target mac are the same... find out why...
	  std::cout << "the target Mac was: " << interest.getInterestTargetMacAddress() << std::endl;
	  std::cout << "currentMacAddress: " << currentMacAddress << std::endl;
	  std::cout << "the new target Mac is: " << it->getTargetMac() << std::endl;

	  if(interest.getInterestTargetMacAddress() == it->getTargetMac()) {
		  std::cout << "should not happen yo: " << shouldNotHappnen++ << std::endl;
		  this->rejectPendingInterest(pitEntry);
	  }

	  if(std::regex_match(it->getTargetMac(), std::regex("([0-9a-fA-F]{2}[:-]){5}[0-9a-fA-F]{2}"))) {
		  const_cast<fib::NextHop&>(*it).incrementCost();
		  targetMac = it->getTargetMac();
	  }


	  if (pitEntry->canForwardTo(*outFace)) {
		  yes++;
		  std::cout << "yes count : " << yes << std::endl;
		this->sendInterest(pitEntry, outFace, targetMac, inFace.getId());
	  } else {
		  i--;
		  no++;
		  std::cout << "no count : " << no << std::endl;
		if(debug) std::cout << "INSIDE MulticastStrategy::afterReceiveInterest can NOT forward to outFace" << std::endl;
	  }
	}

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
