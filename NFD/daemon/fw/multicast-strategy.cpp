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

namespace nfd {
namespace fw {

const Name MulticastStrategy::STRATEGY_NAME("ndn:/localhost/nfd/strategy/multicast/%FD%01");
NFD_REGISTER_STRATEGY(MulticastStrategy);

const bool debug = true;

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

std::cout << std::endl;
for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it) {
	std::cout << "cost: " << it->getCost() << "  ---  mac: " << it->getMac()  << "  ---  within: " << node->GetId() << std::endl;
}
std::cout << std::endl;

int count= 0;

// if node is NOT Consumer
if(interest.getMacAddress() == "consumer") {
	std::cout << "interest->getMacAddress() resulted in TRUE" << std::endl;
} else {
	std::cout << "interest->getMacAddress() resulted in FALSE because it is: " << interest.getMacAddress() << std::endl;
}

//std::cout << "mac out of the interest: " << interest.getMacAddress() << " with name: " << interest.getName() << std::endl;

// TODO here you want to implement the strategy that checks for the incoming mac and it's own mac and
// compares for dropping or continuing.....

  for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it) {
	shared_ptr<Face> outFace = it->getFace();
    if (pitEntry->canForwardTo(*outFace)) {
      //interest.setMacAddress("test");
      this->sendInterest(pitEntry, outFace);
    }
  }

  if (!pitEntry->hasUnexpiredOutRecords()) {
    this->rejectPendingInterest(pitEntry);
  }
}

} // namespace fw
} // namespace nfd
