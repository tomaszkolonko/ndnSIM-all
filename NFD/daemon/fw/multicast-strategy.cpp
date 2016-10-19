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
	const fib::NextHopList& nexthops = fibEntry->getNextHops();
	ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
	std::cout << "you are on node(" << node->GetId() << ") and the Mac of the recv. Interest is: " << interest.getMacAddress() << std::endl;
//	for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it) {
//		std::cout << "cost: " << it->getCost() << "  ---  mac: " << it->getMac()  << "  ---  within: " << node->GetId() << std::endl;
//	}

	printPITInRecord(pitEntry);
	printPITOutRecord(pitEntry);

//	// if node is NOT Consumer
//	if(interest.getMacAddress() == "consumer") {
//		std::cout << "interest->getMacAddress() resulted in TRUE -> consumer" << std::endl;
//	} else {
//		std::cout << "interest->getMacAddress() resulted in FALSE because it is: " << interest.getMacAddress() << std::endl;
//	}

	//std::cout << "mac out of the interest: " << interest.getMacAddress() << " with name: " << interest.getName() << std::endl;

std::cout << "***************** for the next few iterations you are on node (" << node->GetId() << ") !!!" << std::endl;
	  for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it) {
		shared_ptr<Face> outFace = it->getFace();
		std::string targetMac = it->getMac();
		//std::cout << "**FIB** Get Target Mac from FIB entry (" << ++i << "): " << targetMac << std::endl;
		if (pitEntry->canForwardTo(*outFace)) {
		  this->sendInterest(pitEntry, outFace, targetMac);
		} else {
			if(debug) std::cout << "INSIDE MulticastStrategy::afterReceiveInterest can NOT forward to outFace" << std::endl;
		}
	  }

	  if (!pitEntry->hasUnexpiredOutRecords()) {
		this->rejectPendingInterest(pitEntry);
	  }
	}

bool
MulticastStrategy::dropInterest(const Interest& interest, const ns3::Node& node)
{
	if(interest.getMacAddress() == "consumer") {
		std::cout << node.GetId() << std::endl;
		if(node.GetId() == 3 || node.GetId() == 4 || node.GetId() == 5) {
			std::cout << "interest dropped since directly from consumer" << std::endl;
			return true;
		}
	}
	return false;
}

// iterates through all inRecords of ONE pitEntry
// Check FaceRecord for more options to display
void
MulticastStrategy::printPITInRecord(shared_ptr<pit::Entry> pitEntry) {
	const pit::InRecordCollection& inRecCol = pitEntry->getInRecords();
	std::cout << std::endl;
	std::cout << "MulticastStrategy::printPITInRecord" << std::endl;
	for(pit::InRecordCollection::const_iterator inIt = inRecCol.begin(); inIt != inRecCol.end(); ++inIt){
		std::cout << "inRecord interest: " << inIt->getInterest().getName() << std::endl;
		std::cout << "inRecord face: " << inIt->getFace()->getId() << std::endl;
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
	for(pit::OutRecordCollection::const_iterator inIt = outRecCol.begin(); inIt != outRecCol.end(); ++inIt){
		std::cout << "outRecord face: " << inIt->getFace()->getId() << std::endl;
	}
	std::cout << std::endl;
}

} // namespace fw
} // namespace nfd
