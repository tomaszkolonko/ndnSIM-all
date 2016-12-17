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

#include "forwarder.hpp"
#include "core/logger.hpp"
#include "core/random.hpp"
#include "strategy.hpp"
#include "face/null-face.hpp"

// *******
#include "model/ndn-l3-protocol.hpp"
// ************
#include "ns3/ndnSIM/model/ndn-face.hpp"
#include "ns3/net-device.h"
#include "model/ndn-net-device-face.hpp"
// ************
#include "helper/ndn-fib-helper.hpp"
#include "ns3/ndnSIM-module.h"
// #include "model/ndn-net-device-face.hpp"
// *******

#include "utils/ndn-ns3-packet-tag.hpp"

#include "ns3/node.h"
#include "ns3/node-list.h"

#include <boost/random/uniform_int_distribution.hpp>

namespace nfd {

NFD_LOG_INIT("Forwarder");

using fw::Strategy;

const Name Forwarder::LOCALHOST_NAME("ndn:/localhost");


// Needs to start at 1 otherwise dividing by it will lead to an exception
static int node0interest = 1;
static int node1interest = 1;
static int node2interest = 1;
static int node3interest = 1;

static int node0interestDropped = 1;
static int node1interestDropped = 1;
static int node2interestDropped = 1;
static int node3interestDropped = 1;

static int node0data = 1;
static int node1data = 1;
static int node2data = 1;
static int node3data = 1;

static int node0dataDropped = 1;
static int node1dataDropped = 1;
static int node2dataDropped = 1;
static int node3dataDropped = 1;

static int loopDropping = 1;
static int manualDropping = 1;

Forwarder::Forwarder()
  : m_faceTable(*this)
  , m_fib(m_nameTree)
  , m_pit(m_nameTree)
  , m_measurements(m_nameTree)
  , m_strategyChoice(m_nameTree, fw::makeDefaultStrategy(*this))
  , m_csFace(make_shared<NullFace>(FaceUri("contentstore://")))
{
  fw::installStrategies(*this);
  getFaceTable().addReserved(m_csFace, FACEID_CONTENT_STORE);
}

Forwarder::~Forwarder()
{
}

void
Forwarder::onIncomingInterest(Face& inFace, const Interest& interest)
{
	bool debug = false;
  // receive Interest
  NFD_LOG_DEBUG("onIncomingInterest face=" << inFace.getId() <<
                " interest=" << interest.getName());

  const_cast<Interest&>(interest).setIncomingFaceId(inFace.getId());

  ++m_counters.getNInInterests();

  // /localhost scope control
  bool isViolatingLocalhost = !inFace.isLocal() &&
                              LOCALHOST_NAME.isPrefixOf(interest.getName());
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onIncomingInterest face=" << inFace.getId() <<
                  " interest=" << interest.getName() << " violates /localhost");
    // (drop)
    return;
  }

  ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());


  // ******************************************** INTEREST STATS :: START *******************************
  // ****************************************************************************************************
  bool interestStatistics = false;
  if(interestStatistics) {

	  switch(node->GetId()) {
		  case 0: node0interest++; break;
		  case 1: node1interest++; break;
		  case 2: node2interest++; break;
		  case 3: node3interest++; break;
		  default: break;
	  }

	  if(true) {
		  std::cout << "nodeIn" << std::endl;
		  std::cout << "node (0) interest counter: " << node0interest << std::endl;
		  std::cout << "node (1) interest counter: " << node1interest << std::endl;
		  std::cout << "node (2) interest counter: " << node2interest << std::endl;
		  std::cout << "node (3) interest counter: " << node3interest << std::endl;
	  }
  }
  // ******************************************** INTEREST STATS :: END *********************************
  // ****************************************************************************************************


  std::ostringstream addr;
  addr << node->GetDevice(0)->GetAddress();
  std::string currentMacAddress = addr.str().substr(6);

  // check if macAddress has been set. If empty it the forwarding is still broadcasting. If there is a Mac it comes from the fib
  // of the prior node.
  if(interest.getMacAddress() != "consumer " && interest.getMacAddress() != "producer Mac" && interest.getMacAddress() != "unknown"
	  && !interest.getMacAddress().empty()){
	  if(currentMacAddress != interest.getMacAddress()) {
		  return;
	  }
  }

  // ******************************************** INTEREST STATS :: START *******************************
  // ****************************************************************************************************
  if(interestStatistics) {
	  switch(node->GetId()) {
		  case 0: node0interestDropped++; break;
		  case 1: node1interestDropped++; break;
		  case 2: node2interestDropped++; break;
		  case 3: node3interestDropped++; break;
		  default: break;
	  }

	  if(true) {
		  std::cout << "nodeIn" << std::endl;
		  std::cout << "node (0) interest passed counter: " << node0interestDropped << std::endl;
		  std::cout << "node (1) interest passed counter: " << node1interestDropped << std::endl;
		  std::cout << "node (2) interest passed counter: " << node2interestDropped << std::endl;
		  std::cout << "node (3) interest passed counter: " << node3interestDropped << std::endl;
	  }

	  if(true) {
		  std::cout << "vs stat" << std::endl;
		  std::cout << "node (0) interest passed / all: " << (double)node0interestDropped/node0interest*100 << " %" << std::endl;
		  std::cout << "node (1) interest passed / all: " << (double)node1interestDropped/node1interest*100 << " %" << std::endl;
		  std::cout << "node (2) interest passed / all: " << (double)node2interestDropped/node2interest*100 << " %" << std::endl;
		  std::cout << "node (3) interest passed / all: " << (double)node3interestDropped/node3interest*100 << " %" << std::endl;
	  }
  }
  // ******************************************** INTEREST STATS :: END *********************************
  // ****************************************************************************************************

  // PIT insert
  shared_ptr<pit::Entry> pitEntry = m_pit.insert(interest).first;

  // If the an interest with the same nonce has passed this node already, the pit entry
  // will be updated BUT further processing of this interest will be stopped immediately.

  // detect duplicate Nonce
  int dnw = pitEntry->findNonce(interest.getNonce(), inFace);
  bool hasDuplicateNonce = (dnw != pit::DUPLICATE_NONCE_NONE) ||
                           m_deadNonceList.has(interest.getName(), interest.getNonce());
  if (hasDuplicateNonce) {
    // goto Interest loop pipeline which does nothing at the moment.
    this->onInterestLoop(inFace, interest, pitEntry);
    loopDropping++;
    std::cout << "loopDropping: " << loopDropping << " "	;
    // drop the package
    return;
  }

  if(debug) {
	  std::cout << std::endl;
	  std::cout << "you are on node(" << node->GetId() << ") and the interest is: " << interest.getName() << std::endl;
	  std::cout << "the interest path is: " << interest.getMacAddressPath() << std::endl;
  }

  // cancel unsatisfy & straggler timer
  this->cancelUnsatisfyAndStragglerTimer(pitEntry);

  // is pending?
  const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
  bool isPending = inRecords.begin() != inRecords.end();
  if (!isPending) {
    if (m_csFromNdnSim == nullptr) {
      m_cs.find(interest,
                bind(&Forwarder::onContentStoreHit, this, ref(inFace), pitEntry, _1, _2),
                bind(&Forwarder::onContentStoreMiss, this, ref(inFace), pitEntry, _1));
    }
    else {
      shared_ptr<Data> match = m_csFromNdnSim->Lookup(interest.shared_from_this());
      if (match != nullptr) {
        this->onContentStoreHit(inFace, pitEntry, interest, *match);
      }
      else {
        this->onContentStoreMiss(inFace, pitEntry, interest);
      }
    }
  }
  else {
    this->onContentStoreMiss(inFace, pitEntry, interest);
  }
}

void
Forwarder::onContentStoreMiss(const Face& inFace,
                              shared_ptr<pit::Entry> pitEntry,
                              const Interest& interest)
{
  NFD_LOG_DEBUG("onContentStoreMiss interest=" << interest.getName());

  shared_ptr<Face> face = const_pointer_cast<Face>(inFace.shared_from_this());

  // insert InRecord
  pitEntry->insertOrUpdateInRecord(face, interest);

  // set PIT unsatisfy timer
  this->setUnsatisfyTimer(pitEntry);

  // FIB lookup
  shared_ptr<fib::Entry> fibEntry = m_fib.findLongestPrefixMatch(*pitEntry);

  // Displays all the nextHops for a certain prefix on a certain node IMPORTANT
  // at the moment all new routes get the 04 MAC address which is a big problem
  ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());


  if(true) {
	  std::cout << std::endl;
	  std::cout << "INSIDE Forwarder::OnContentStoreMiss" << std::endl;
	  const fib::NextHopList& nexthops = fibEntry->getNextHops();
	  std::cout << "within ONE fibEntry for prefix: \"" << fibEntry->getPrefix() << "\" on node (" << node->GetId()
												<< ") looking at nextHop entries...." << std::endl;
	  for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it) {
		  std::cout << "    - > Get fibEntry outFace" << it->getFace()->getId() << "   Cost: " << it->getCost() << "   Mac: " << it->getMac() << std::endl;
	  }
	  std::cout << std::endl;
  }
//
//  if(debug) {
//	  std::cout << "inRecordCollection: node(" << node->GetId() << ") ";
//	  const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
//	  for (pit::InRecordCollection::const_iterator it = inRecords.begin();
//													 it != inRecords.end(); ++it) {
//		  std::cout << "inside onContentstoreMiss: " << it->getFace()->getId() << "; ";
//	  }
//  }


  // dispatch to strategy
  this->dispatchToStrategy(pitEntry, bind(&Strategy::afterReceiveInterest, _1,
                                          cref(inFace), cref(interest), fibEntry, pitEntry));
}

void
Forwarder::onContentStoreHit(const Face& inFace,
                             shared_ptr<pit::Entry> pitEntry,
                             const Interest& interest,
                             const Data& data)
{
  NFD_LOG_DEBUG("onContentStoreHit interest=" << interest.getName());

  beforeSatisfyInterest(*pitEntry, *m_csFace, data);
  this->dispatchToStrategy(pitEntry, bind(&Strategy::beforeSatisfyInterest, _1,
                                          pitEntry, cref(*m_csFace), cref(data)));

  const_pointer_cast<Data>(data.shared_from_this())->setIncomingFaceId(FACEID_CONTENT_STORE);
  // XXX should we lookup PIT for other Interests that also match csMatch?

  // set PIT straggler timer
  this->setStragglerTimer(pitEntry, true, data.getFreshnessPeriod());

  // goto outgoing Data pipeline
  this->onOutgoingData(data, *const_pointer_cast<Face>(inFace.shared_from_this()));
}

void
Forwarder::onInterestLoop(Face& inFace, const Interest& interest,
                          shared_ptr<pit::Entry> pitEntry)
{
  NFD_LOG_DEBUG("onInterestLoop face=" << inFace.getId() <<
                " interest=" << interest.getName());

  // (drop)
}

/** \brief compare two InRecords for picking outgoing Interest
 *  \return true if b is preferred over a
 *
 *  This function should be passed to std::max_element over InRecordCollection.
 *  The outgoing Interest picked is the last incoming Interest
 *  that does not come from outFace.
 *  If all InRecords come from outFace, it's fine to pick that. This happens when
 *  there's only one InRecord that comes from outFace. The legit use is for
 *  vehicular network; otherwise, strategy shouldn't send to the sole inFace.
 */
static inline bool
compare_pickInterest(const pit::InRecord& a, const pit::InRecord& b, const Face* outFace)
{
  bool isOutFaceA = a.getFace().get() == outFace;
  bool isOutFaceB = b.getFace().get() == outFace;

  if (!isOutFaceA && isOutFaceB) {
    return false;
  }
  if (isOutFaceA && !isOutFaceB) {
    return true;
  }

  return a.getLastRenewed() > b.getLastRenewed();
}

void
Forwarder::onOutgoingInterest(shared_ptr<pit::Entry> pitEntry, Face& outFace,
                              bool wantNewNonce)
{
  if (outFace.getId() == INVALID_FACEID) {
    NFD_LOG_WARN("onOutgoingInterest face=invalid interest=" << pitEntry->getName());
    return;
  }
  NFD_LOG_DEBUG("onOutgoingInterest face=" << outFace.getId() <<
                " interest=" << pitEntry->getName());

  // scope control
  if (pitEntry->violatesScope(outFace)) {
    NFD_LOG_DEBUG("onOutgoingInterest face=" << outFace.getId() <<
                  " interest=" << pitEntry->getName() << " violates scope");
    return;
  }

  // pick Interest
  const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
  pit::InRecordCollection::const_iterator pickedInRecord = std::max_element(
    inRecords.begin(), inRecords.end(), bind(&compare_pickInterest, _1, _2, &outFace));
  BOOST_ASSERT(pickedInRecord != inRecords.end());
  shared_ptr<Interest> interest = const_pointer_cast<Interest>(
    pickedInRecord->getInterest().shared_from_this());

  if (wantNewNonce) {
    interest = make_shared<Interest>(*interest);
    static boost::random::uniform_int_distribution<uint32_t> dist;
    interest->setNonce(dist(getGlobalRng()));
  }

  // insert OutRecord
  pitEntry->insertOrUpdateOutRecord(outFace.shared_from_this(), *interest);

  ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());

  // send Interest
  outFace.sendInterest(*interest);
  ++m_counters.getNOutInterests();
}

void
Forwarder::onOutgoingInterest(shared_ptr<pit::Entry> pitEntry, Face& outFace,
                              std::string targetMac, int inFaceId, bool wantNewNonce)
{
	bool debug = false;
  if (outFace.getId() == INVALID_FACEID) {
    NFD_LOG_WARN("onOutgoingInterest face=invalid interest=" << pitEntry->getName());
    return;
  }
  NFD_LOG_DEBUG("onOutgoingInterest face=" << outFace.getId() <<
                " interest=" << pitEntry->getName());

  // scope control
  if (pitEntry->violatesScope(outFace)) {
    NFD_LOG_DEBUG("onOutgoingInterest face=" << outFace.getId() <<
                  " interest=" << pitEntry->getName() << " violates scope");
    return;
  }

  // pick Interest
  const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
  pit::InRecordCollection::const_iterator pickedInRecord = std::max_element(
    inRecords.begin(), inRecords.end(), bind(&compare_pickInterest, _1, _2, &outFace));
  BOOST_ASSERT(pickedInRecord != inRecords.end());
  shared_ptr<Interest> interest = const_pointer_cast<Interest>(
    pickedInRecord->getInterest().shared_from_this());

  if (wantNewNonce) {
    interest = make_shared<Interest>(*interest);
    static boost::random::uniform_int_distribution<uint32_t> dist;
    interest->setNonce(dist(getGlobalRng()));
  }

  ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());

  // insert OutRecord
  pitEntry->insertOrUpdateOutRecord(outFace.shared_from_this(), *interest);


  // ***************** ADDING MAC ADDRESS TO PATH ON INTEREST :: BEGIN ***************************
  // *********************************************************************************************
  ns3::Address ad;
  ad = node->GetDevice(0)->GetAddress();
  std::ostringstream breadcrumbInterest;
  breadcrumbInterest << ad;
  std::string breadcrumbInterest_string = breadcrumbInterest.str().substr(6);
  std::string interest_macAddressPath = interest->getMacAddressPath();

  // check if this device's mac address has been added already to the interest. If yes the position of the
  // start position will be returned if the madAddress has not yet been added to the interest the function
  // will return std::string::npos
  std::size_t found = interest_macAddressPath.find(breadcrumbInterest_string);

  if(found == std::string::npos) {

	  // don't attach the current Mac again to the Path if it has been attached by a previous
	  std::string in = " (in " + std::to_string(node->GetId()) + "/" + std::to_string(inFaceId) + ") ";
	  std::string out = " (out " + std::to_string(node->GetId()) + "/" + std::to_string(outFace.getId()) + ") ";

	  interest->addMacAddressPath(" --> " +  in + breadcrumbInterest.str().substr(6) + out);
  }
  // ***************** ADDING MAC ADDRESS TO PATH ON INTEREST :: END *****************************
  // *********************************************************************************************


  shared_ptr<Interest> interest2 = make_shared<Interest>(*interest);

  // Sample Output of this:
  // Explanation: 	you have an interest and a copied interest pointing to two different locations.
  //				They won't be changed but iterated over and over again leading to interests having
  //				very long MadAddressPath variables... The don't loop within the node but this mac
  //				is written to the field again and again for ever outFace.

//  pointer of interest /test/prefix/%FE%00 outFace: 256 0x12dbb60
//  pointer of interest2 /test/prefix/%FE%00 outFace: 256 0x12f0380
//  pointer of interest /test/prefix/%FE%00 outFace: 257 0x12dbb60
//  pointer of interest2 /test/prefix/%FE%00 outFace: 257 0x12f0380
//  pointer of interest /test/prefix/%FE%00 outFace: 258 0x12dbb60
//  pointer of interest2 /test/prefix/%FE%00 outFace: 258 0x12f0380
//  pointer of interest /test/prefix/%FE%00 outFace: 259 0x12dbb60
//  pointer of interest2 /test/prefix/%FE%00 outFace: 259 0x12f0380
//  pointer of interest /test/prefix/%FE%00 outFace: 260 0x12dbb60
//  pointer of interest2 /test/prefix/%FE%00 outFace: 260 0x12f0380
//  pointer of interest /test/prefix/%FE%00 outFace: 261 0x12dbb60
//  pointer of interest2 /test/prefix/%FE%00 outFace: 261 0x12f0380

  if(debug) {
	  std::cout << "pointer of interest " << interest->getName() << " outFace: " << outFace.getId() <<  " " << interest << std::endl;
	  std::cout << "pointer of interest2 " << interest2->getName() << " outFace: " << outFace.getId() << " " << interest2 << std::endl;
  }

  interest2->setMacAddress(targetMac);
  outFace.sendInterest(*interest2);
  ++m_counters.getNOutInterests();
}

void
Forwarder::onInterestReject(shared_ptr<pit::Entry> pitEntry)
{
  if (pitEntry->hasUnexpiredOutRecords()) {
    NFD_LOG_ERROR("onInterestReject interest=" << pitEntry->getName() <<
                  " cannot reject forwarded Interest");
    return;
  }
  NFD_LOG_DEBUG("onInterestReject interest=" << pitEntry->getName());

  // cancel unsatisfy & straggler timer
  this->cancelUnsatisfyAndStragglerTimer(pitEntry);

  // set PIT straggler timer
  this->setStragglerTimer(pitEntry, false);
}

void
Forwarder::onInterestUnsatisfied(shared_ptr<pit::Entry> pitEntry)
{
  NFD_LOG_DEBUG("onInterestUnsatisfied interest=" << pitEntry->getName());

  // invoke PIT unsatisfied callback
  beforeExpirePendingInterest(*pitEntry);
  this->dispatchToStrategy(pitEntry, bind(&Strategy::beforeExpirePendingInterest, _1,
                                          pitEntry));

  // goto Interest Finalize pipeline
  this->onInterestFinalize(pitEntry, false);
}

void
Forwarder::onInterestFinalize(shared_ptr<pit::Entry> pitEntry, bool isSatisfied,
                              const time::milliseconds& dataFreshnessPeriod)
{
  NFD_LOG_DEBUG("onInterestFinalize interest=" << pitEntry->getName() <<
                (isSatisfied ? " satisfied" : " unsatisfied"));

  // Dead Nonce List insert if necessary
  this->insertDeadNonceList(*pitEntry, isSatisfied, dataFreshnessPeriod, 0);

  // PIT delete
  this->cancelUnsatisfyAndStragglerTimer(pitEntry);
  m_pit.erase(pitEntry);
}

void
Forwarder::onIncomingData(Face& inFace, const Data& data)
{
  bool debug = false;

  if(true) {
    std::cout << "\n* > * > * > * > * > * > * > * > * > * > * > * > * > * > * > * > * > * > * > * > * > *\n";
    std::cout << "data.getMetaInfo(): " << data.getMetaInfo() << std::endl;
    std::cout << "data.getName(): " << data.getName() << std::endl;
  }

  // receive Data
  NFD_LOG_DEBUG("onIncomingData face=" << inFace.getId() << " data=" << data.getName());
  const_cast<Data&>(data).setIncomingFaceId(inFace.getId());
  ++m_counters.getNInDatas();

  // /localhost scope control
  bool isViolatingLocalhost = !inFace.isLocal() &&
                              LOCALHOST_NAME.isPrefixOf(data.getName());
  std::cout << "isViolatingLocalhost: " << isViolatingLocalhost << std::endl;
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onIncomingData face=" << inFace.getId() <<
                  " data=" << data.getName() << " violates /localhost");
    // (drop)
    return;
  }

  // PIT match
  pit::DataMatchResult pitMatches = m_pit.findAllDataMatches(data);

  std::cout << "First: pitMatches.size(): " << pitMatches.size() << std::endl;

  // all packages are dropped if they were not requested before
  if (pitMatches.begin() == pitMatches.end()) {
    // goto Data unsolicited pipeline
    // this->onDataUnsolicited(inFace, data);
    return;
  }

  std::cout << "Second: pitMatches.size(): " << pitMatches.size() << std::endl;

  // Try to achieve 3 hops as it should be during the scenario -> follow breadcrumbs!!!
  // At the moment all data.Mac's are 04 empty or producers

  ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
  std::cout << "node( " << node->GetId() << ")" << std::endl;
  std::cout << "* < * < * < * < * < * < * < * < * < * < * < * < * < * < * < * < * < * < * < * < * < * < * <\n";


  if(debug) {
	  std::cout << "yeah --- 2 --- on node(    "<< node->GetId() <<"    )  pitchMatches.size() is: " << pitMatches.size() << std::endl;
  }

  // ******************************************** DATA STATS :: START ***********************************
  // ****************************************************************************************************
  bool dataStatistics = false;
  if(dataStatistics) {
	  switch(node->GetId()) {
		  case 0: node0data++; break;
		  case 1: node1data++; break;
		  case 2: node2data++; break;
		  case 3: node3data++; break;
		  default: break;
	  }

	  if(true) {
		  std::cout << "nodeDa" << std::endl;
		  std::cout << "node (0) data counter: " << node0data << std::endl;
		  std::cout << "node (1) data counter: " << node1data << std::endl;
		  std::cout << "node (2) data counter: " << node2data << std::endl;
		  std::cout << "node (3) data counter: " << node3data << std::endl;
	  }
  }
  // ******************************************** DATA STATS :: END *******************************
  // ****************************************************************************************************

  // ********************* Check all in and out Records of specific PIT::Entry -- BEGIN **********************************
  // *********************************************************************************************************************
  if(false) {
	  std::cout << "123123123123123123123123123123123123123123123 on node(" << node->GetId() << ") and inFace: " <<
			  inFace.getId() << std::endl;
	  for (const shared_ptr<pit::Entry>& pitEntry : pitMatches) {
		  std::cout << pitEntry->getName() << std::endl;

		  // IN RECORD COLLECTION
		  std::cout << "inRecordCollection:";
		  const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
		  for (pit::InRecordCollection::const_iterator it = inRecords.begin();
														 it != inRecords.end(); ++it) {
			  std::cout << " " << it->getFace()->getId() << "; ";
		  }
		  std::cout << std::endl;

		  // OUT RECORD COLLECTION
		  std::cout << "outRecordCollection:";
		  const pit::OutRecordCollection& outRecords = pitEntry->getOutRecords();
		  for (pit::OutRecordCollection::const_iterator it = outRecords.begin();
														 it != outRecords.end(); ++it) {
			  std::cout << " " << it->getFace()->getId() << "; ";
		  }
	  }
	  std::cout << "\n123123123123123123123123123123123123123123123" << std::endl;
  }
  // ********************* Check all in and out Records of specific PIT::Entry -- END **********************************
  // *******************************************************************************************************************


  // ******************************************** DATA STATS :: START ***********************************
  // ****************************************************************************************************
  if(dataStatistics) {
	  switch(node->GetId()) {
		  case 0: node0dataDropped++; break;
		  case 1: node1dataDropped++; break;
		  case 2: node2dataDropped++; break;
		  case 3: node3dataDropped++; break;
		  default: break;
	  }

	  if(true) {
		  std::cout << "nodeDa" << std::endl;
		  std::cout << "node (0) data passed counter: " << node0dataDropped << std::endl;
		  std::cout << "node (1) data passed counter: " << node1dataDropped << std::endl;
		  std::cout << "node (2) data passed counter: " << node2dataDropped << std::endl;
		  std::cout << "node (3) data passed counter: " << node3dataDropped << std::endl;
	  }

	  if(true) {
		  std::cout << "vs stat" << std::endl;
		  std::cout << "node (0) data passed / all: " << (double)node0dataDropped/node0data*100 << " %" << std::endl;
		  std::cout << "node (1) data passed / all: " << (double)node1dataDropped/node1data*100 << " %" << std::endl;
		  std::cout << "node (2) data passed / all: " << (double)node2dataDropped/node2data*100 << " %" << std::endl;
		  std::cout << "node (3) data passed / all: " << (double)node3dataDropped/node3data*100 << " %" << std::endl;
	  }
  }
  // ******************************************** DATA STATS :: END *******************************
  // ****************************************************************************************************

  // some logic on where and how to add new Routes using NO predefined MacAddresses
  if(data.getMacAddressPro() == "producer Mac") {
	  ns3::ndn::FibHelper::AddRoute(node, "/", inFace.getId(), 12, data.getMacAddressPro());
  } else if(data.getMacAddressPro().empty()){
	  // do nothing. That only happens at configuration time and never again.
  } else {

  //	  ns3::Ptr<ns3::NetDevice> netDevice = node->GetDevice(0);
  //	  ns3::Ptr<ns3::ndn::L3Protocol> l3 = node->GetObject<ns3::ndn::L3Protocol>();
  //
  //	  // https://groups.google.com/forum/#!topic/ns-3-users/g4kgUdXmVUg
  //
  //	  ns3::ndn::NetDeviceFace* p_netDF = new ns3::ndn::NetDeviceFace(node, netDevice);
  //	  l3->addFace((shared_ptr<ns3::ndn::Face>)p_netDF);

	   // ns3::ndn::FibHelper::RemoveRoute(node, "/test", inFace.getId());

	  ns3::ndn::FibHelper::AddRoute(node, "/", inFace.getId(), 111, data.getMacAddressPro());
  }

  // Remove Ptr<Packet> from the Data before inserting into cache, serving two purposes
  // - reduce amount of memory used by cached entries
  // - remove all tags that (e.g., hop count tag) that could have been associated with Ptr<Packet>
  //
  // Copying of Data is relatively cheap operation, as it copies (mostly) a collection of Blocks
  // pointing to the same underlying memory buffer.
  shared_ptr<Data> dataCopyWithoutPacket = make_shared<Data>(data);
  dataCopyWithoutPacket->removeTag<ns3::ndn::Ns3PacketTag>();

  // CS insert
  if (m_csFromNdnSim == nullptr)
    m_cs.insert(*dataCopyWithoutPacket);
  else
    m_csFromNdnSim->Add(dataCopyWithoutPacket);

  std::set<shared_ptr<Face>> pendingDownstreams;
  // foreach PitEntry

  if(debug) std::cout << "pitMatches.size() : " << pitMatches.size() << std::endl;


  // *************** TAKE PIT ENTRY AND EXTRACT NEEDED FACES ***********************************
  // *******************************************************************************************
  for (const shared_ptr<pit::Entry>& pitEntry : pitMatches) {

    // cancel unsatisfy & straggler timer
    this->cancelUnsatisfyAndStragglerTimer(pitEntry);

    // remember pending downstreams
    const pit::InRecordCollection& inRecords = pitEntry->getInRecords();

    if(true) std::cout << "inRecords.size() for each entry within pitMatches" << inRecords.size() << std::endl;

    for (pit::InRecordCollection::const_iterator it = inRecords.begin();
                                                 it != inRecords.end(); ++it) {
      if (it->getExpiry() > time::steady_clock::now()) {
    	  // TODO: here seems to be a problem. Rarely a face (256) is added to pendingDownstream.
    	  // TODO: find out why? BECAUSE THE SAME PIT ENTRY IS USED AND DELETED FEW LINES DOWN
    	  std::cout << "pendingDownstreams.instert(it->getFace()) :  " << it->getFace()->getId() << std::endl;
        pendingDownstreams.insert(it->getFace());
      }
    }
  // *************** TAKE PIT ENTRY AND EXTRACT NEEDED FACES ***********************************
  // *******************************************************************************************

    // invoke PIT satisfy callback
    beforeSatisfyInterest(*pitEntry, inFace, data);
    // ATTENTION: EVERY PIT ENTRY THAT IS ITERATED THROUGH WILL BE DISPACHED AND DELETED!!!!
    this->dispatchToStrategy(pitEntry, bind(&Strategy::beforeSatisfyInterest, _1,
                                            pitEntry, cref(inFace), cref(data)));

    // Dead Nonce List insert if necessary (for OutRecord of inFace)
    this->insertDeadNonceList(*pitEntry, true, data.getFreshnessPeriod(), &inFace);

    // mark PIT satisfied
    // ATTENTION: EVERY PIT ENTRY THAT IS ITERATED THROUGH WILL BE DISPACHED AND DELETED!!!!
    pitEntry->deleteInRecords();
    // TODO: understand why pitEntry->deleteOutRecord() needs inFace... is it possible to have
    // per pitEntry several outRecords that are not deleted while all inRecords do get deleted???
    pitEntry->deleteOutRecord(inFace);

    // set PIT straggler timer
    // The purpose of the straggler timer is to keep PIT entry alive for a short
    // period of time in order to facilitate duplicate Interest detection and to
    // collect data plane measurements.
    this->setStragglerTimer(pitEntry, true, data.getFreshnessPeriod());
  }

  // foreach pending downstream
  if(debug) std::cout << "ALPHA you are on node(" << node->GetId()
		  << " and the pendingDownstream size is: " << pendingDownstreams.size() << std::endl;

  for (std::set<shared_ptr<Face> >::iterator it = pendingDownstreams.begin();
		  	  	  it != pendingDownstreams.end(); ++it) {
	  shared_ptr<Face> pendingDownstream = *it;

	  if(debug) {
		  std::cout << "BETA pendingDownstream.get()->getId(): " << pendingDownstream.get()->getId() << std::endl;
		  std::cout << "BETA inFace through which the data came from: " << inFace.getId() << std::endl;
	  }

	  // ********************** some logic how to add more faces to the downstream *************************************
	  // ***************************************************************************************************************
		if(inFace.getId() == 256 || inFace.getId() == 257 || inFace.getId() == 258 || inFace.getId() == 259
						|| inFace.getId() == 260 || inFace.getId() == 261 || inFace.getId() == 263) {
			if(debug) std::cout << "WITHIN IF: : pitMatches.size() : : " << pitMatches.size() << std::endl;
			  for (const shared_ptr<pit::Entry>& pitEntry : pitMatches) {
				  if(debug) std::cout << std::endl;
				  // OUT RECORD COLLECTION
				  //std::cout << "outRecordCollection:";
				  const pit::OutRecordCollection& outRecords = pitEntry->getOutRecords();
				  std::cout << "OutRecordCollection.size() -> " << outRecords.size() << std::endl;
				  std::cout << "for the follwing pitEntry: " << pitEntry << " on node " << node->GetId() << std::endl;
				  for (pit::OutRecordCollection::const_iterator it = outRecords.begin();
																 it != outRecords.end(); ++it) {
					  if(debug) std::cout << "sending data out on node(" << node->GetId() << ") WITHIN NODE it->getFace(): " << it->getFace()->getId() << std::endl;
					  // pitEntry->canForwardTo(*it->getFace()) leads always to FALSE
					  // beware that outRecordCollection is of PIT::ENTRY
					  // inside MulticastStrategy NextHops of FIB given to pitEntry->canForwardTo(fib::nextHops...)
					  this->onOutgoingData(data, *it->getFace());

				  }
			  }
		}
	  // ********************** some logic how to add more faces to the downstream *************************************
	  // ***************************************************************************************************************


	  // data cannot be send through the same face it was received on
	  if (pendingDownstream.get() == &inFace) {
		  if(debug) std::cout << " --> pendingDownstream.get() == &inFace <--  WHICH IS NOT TOO GOOD" << std::endl;
		  continue;
	  }
	  // std::cout << "you are on node (" << node->GetId() << ") AND PENDINGDOWNSTREAM IS OK ;) inFace: " << inFace.getId() << std::endl;
    // Get the current node and get every NetDevice on it. Then get the FaceId through l3protocol and compare
    // it to the downstream face if it matches attach it to the MacField in data for next node
    // ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());

//    for(u_int i = 0; i < node->GetNDevices(); i++) {
//    	ns3::Ptr<ns3::NetDevice> netDevice = node->GetDevice(i);
//    	ns3::Ptr<ns3::ndn::L3Protocol> l3 = node->GetObject<ns3::ndn::L3Protocol>();
//    	// TODO: Problem here is that you have more faces for faceToCheck not only one!!!!!!!!!!!!!!!!
//    	// What about the faceTable?
//    	shared_ptr<Face> faceToCheck = l3->getFaceByNetDevice(netDevice);
//		ad = netDevice->GetAddress();
//		std::ostringstream str;
//		str << ad;
//    	// If true then attach the Mac of the NetDevice to the data package in order to put it into FIB on next node
//    	if(faceToCheck->getId() == pendingDownstream->getId()) {
//    		shared_ptr<Data> dataWithNewMac = make_shared<Data>(data);
//    		dataWithNewMac->setMacAddressPro(str.str().substr(6));
//    		dataWithNewMac->addMacRoute(" --> " + str.str().substr(6));
////    		if(true	) {
////    		  std::cout << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%" << std::endl;
////    		  std::cout << "you are on node (" << node->GetId() << ")" << std::endl;
////    		  std::cout << "Mac on the NEW data package is: " << dataWithNewMac->getMacAddressPro() << std::endl;
////    		  std::cout << "It should be the same as one of the nodes NetDevice Macs" << std::endl;;
////    		  std::cout << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%" << std::endl;
////    		}
//    		this->onOutgoingData(*dataWithNewMac, *pendingDownstream);
//    	} else {
//    	    shared_ptr<Data> dataWithNewMac = make_shared<Data>(data);
//    	    dataWithNewMac->setMacAddressPro(str.str().substr(6));
//    	    dataWithNewMac->addMacRoute(" --> " + str.str().substr(6));
//    	    this->onOutgoingData(*dataWithNewMac, *pendingDownstream);
//    	}
//    }


	if(debug) std::cout << "sending data out on node(" << node->GetId() << ") regular *pendingDownstream with it->getFace(): " <<
			pendingDownstream->getId() << std::endl;
	this->onOutgoingData(data, *pendingDownstream);
	if(debug) std::cout << "\n* < * < * < * < * < * < * < * < * < * < * < * < * < * < * < * < * < * < * < * < * < *\n";
 }
}

void
Forwarder::onDataUnsolicited(Face& inFace, const Data& data)
{
  // std::cout << "Entered Forwarder::onDataUnsolicited" << std::endl;
  // accept to cache?
  bool acceptToCache = inFace.isLocal();
  if (acceptToCache) {
    // CS insert
    if (m_csFromNdnSim == nullptr)
      m_cs.insert(data, true);
    else
      m_csFromNdnSim->Add(data.shared_from_this());
  }
  NFD_LOG_DEBUG("onDataUnsolicited face=" << inFace.getId() <<
                " data=" << data.getName() <<
                (acceptToCache ? " cached" : " not cached"));
}

void
Forwarder::onOutgoingData(const Data& data, Face& outFace)
{
	bool debug = false;
  if (outFace.getId() == INVALID_FACEID) {
    NFD_LOG_WARN("onOutgoingData face=invalid data=" << data.getName());
    return;
  }
  NFD_LOG_DEBUG("onOutgoingData face=" << outFace.getId() << " data=" << data.getName());

  // /localhost scope control
  bool isViolatingLocalhost = !outFace.isLocal() &&
                              LOCALHOST_NAME.isPrefixOf(data.getName());
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onOutgoingData face=" << outFace.getId() <<
                  " data=" << data.getName() << " violates /localhost");
    // (drop)
    return;
  }

	// goto outgoing Data pipeline
	ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());
	ns3::Ptr<ns3::NetDevice> netDevice = node->GetDevice(0);
	std::ostringstream str;
	str << netDevice->GetAddress();

	if(debug) std::cout << "pointer to netDevice : " << netDevice << " with address: " << str.str() << std::endl;

	const_cast<Data&>(data).setMacAddressPro(str.str().substr(6));
	const_cast<Data&>(data).addMacRoute(" --> " + str.str().substr(6));

  // TODO traffic manager

  // send Data
  outFace.sendData(data);
  ++m_counters.getNOutDatas();
}

static inline bool
compare_InRecord_expiry(const pit::InRecord& a, const pit::InRecord& b)
{
  return a.getExpiry() < b.getExpiry();
}

void
Forwarder::setUnsatisfyTimer(shared_ptr<pit::Entry> pitEntry)
{
  const pit::InRecordCollection& inRecords = pitEntry->getInRecords();
  pit::InRecordCollection::const_iterator lastExpiring =
    std::max_element(inRecords.begin(), inRecords.end(),
    &compare_InRecord_expiry);

  time::steady_clock::TimePoint lastExpiry = lastExpiring->getExpiry();
  time::nanoseconds lastExpiryFromNow = lastExpiry  - time::steady_clock::now();
  if (lastExpiryFromNow <= time::seconds(0)) {
    // TODO all InRecords are already expired; will this happen?
  }

  scheduler::cancel(pitEntry->m_unsatisfyTimer);
  pitEntry->m_unsatisfyTimer = scheduler::schedule(lastExpiryFromNow,
    bind(&Forwarder::onInterestUnsatisfied, this, pitEntry));
}

void
Forwarder::setStragglerTimer(shared_ptr<pit::Entry> pitEntry, bool isSatisfied,
                             const time::milliseconds& dataFreshnessPeriod)
{
  time::nanoseconds stragglerTime = time::milliseconds(100);

  scheduler::cancel(pitEntry->m_stragglerTimer);
  pitEntry->m_stragglerTimer = scheduler::schedule(stragglerTime,
    bind(&Forwarder::onInterestFinalize, this, pitEntry, isSatisfied, dataFreshnessPeriod));
}

void
Forwarder::cancelUnsatisfyAndStragglerTimer(shared_ptr<pit::Entry> pitEntry)
{
  scheduler::cancel(pitEntry->m_unsatisfyTimer);
  scheduler::cancel(pitEntry->m_stragglerTimer);
}

static inline void
insertNonceToDnl(DeadNonceList& dnl, const pit::Entry& pitEntry,
                 const pit::OutRecord& outRecord)
{
  dnl.add(pitEntry.getName(), outRecord.getLastNonce());
}

void
Forwarder::insertDeadNonceList(pit::Entry& pitEntry, bool isSatisfied,
                               const time::milliseconds& dataFreshnessPeriod,
                               Face* upstream)
{
  // need Dead Nonce List insert?
  bool needDnl = false;
  if (isSatisfied) {
    bool hasFreshnessPeriod = dataFreshnessPeriod >= time::milliseconds::zero();
    // Data never becomes stale if it doesn't have FreshnessPeriod field
    needDnl = static_cast<bool>(pitEntry.getInterest().getMustBeFresh()) &&
              (hasFreshnessPeriod && dataFreshnessPeriod < m_deadNonceList.getLifetime());
  }
  else {
    needDnl = true;
  }

  if (!needDnl) {
    return;
  }

  // Dead Nonce List insert
  if (upstream == 0) {
    // insert all outgoing Nonces
    const pit::OutRecordCollection& outRecords = pitEntry.getOutRecords();
    std::for_each(outRecords.begin(), outRecords.end(),
                  bind(&insertNonceToDnl, ref(m_deadNonceList), cref(pitEntry), _1));
  }
  else {
    // insert outgoing Nonce of a specific face
    pit::OutRecordCollection::const_iterator outRecord = pitEntry.getOutRecord(*upstream);
    if (outRecord != pitEntry.getOutRecords().end()) {
      m_deadNonceList.add(pitEntry.getName(), outRecord->getLastNonce());
    }
  }
}

} // namespace nfd
