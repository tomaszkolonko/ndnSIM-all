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

static int producerMacSemaphore = 0;

// static unsigned int semaphoreAlternate = NET_DEVICE_ZERO;

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

  int numberOfNetDevices = node->GetNDevices();
  std::ostringstream addr[numberOfNetDevices];
  std::string currentMacAddresses[numberOfNetDevices];

  for(int index = 0; index < numberOfNetDevices; index++) {
	  addr[index] << node->GetDevice(index)->GetAddress();
	  currentMacAddresses[index] = addr[index].str().substr(6);
  }

  // ALTHOUGH THIS SHOULD BE OK -> LEADS TO LESS DATA COMING BACK ;(
//	  if(interest.getInterestOriginMacAddress() == currentMacAddresses[0]
//	                    || interest.getInterestOriginMacAddress() == currentMacAddresses[1]) {
//		  return;
//	  }

  if(std::regex_match(interest.getInterestTargetMacAddress(), std::regex("([0-9a-fA-F]{2}[:-]){5}[0-9a-fA-F]{2}"))) {
	  if(currentMacAddresses[0] != interest.getInterestTargetMacAddress()
			  && currentMacAddresses[1] != interest.getInterestTargetMacAddress()
			  && currentMacAddresses[2] != interest.getInterestTargetMacAddress()
			  && currentMacAddresses[3] != interest.getInterestTargetMacAddress()) {
		return;
	  }
  }

  // PIT insert
  shared_ptr<pit::Entry> pitEntry = m_pit.insert(interest).first;

  // detect duplicate Nonce
  int dnw = pitEntry->findNonce(interest.getNonce(), inFace);
  bool hasDuplicateNonce = (dnw != pit::DUPLICATE_NONCE_NONE) ||
                           m_deadNonceList.has(interest.getName(), interest.getNonce());
  if (hasDuplicateNonce) {
    this->onInterestLoop(inFace, interest, pitEntry);
    // drop the package
    return;
  }

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
  pitEntry->insertOrUpdateOriginMacRecord(interest.getInterestOriginMacAddress(), interest);

  // set PIT unsatisfy timer
  this->setUnsatisfyTimer(pitEntry);

  // FIB lookup
  shared_ptr<fib::Entry> fibEntry = m_fib.findLongestPrefixMatch(*pitEntry);

  ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());

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
  shared_ptr<Face> face = const_pointer_cast<Face>(inFace.shared_from_this());
  pitEntry->insertOrUpdateInRecord(face, interest);
    pitEntry->insertOrUpdateOriginMacRecord(interest.getInterestOriginMacAddress(), interest);
  beforeSatisfyInterest(*pitEntry, *m_csFace, data);
  this->dispatchToStrategy(pitEntry, bind(&Strategy::beforeSatisfyInterest, _1,
                                          pitEntry, cref(*m_csFace), cref(data)));

  const_pointer_cast<Data>(data.shared_from_this())->setIncomingFaceId(FACEID_CONTENT_STORE);
  // XXX should we lookup PIT for other Interests that also match csMatch?

  // set PIT straggler timer
  this->setStragglerTimer(pitEntry, true, data.getFreshnessPeriod());
//data.setDataTargetMacAddress(interest.getInterestOriginMacAddress());
  // goto outgoing Data pipeline
  this->onOutgoingData(data, *const_pointer_cast<Face>(inFace.shared_from_this()), interest.getInterestOriginMacAddress(), interest.getInterestTargetMacAddress());
}

void
Forwarder::onInterestLoop(Face& inFace, const Interest& interest,
                          shared_ptr<pit::Entry> pitEntry)
{
  NFD_LOG_DEBUG("onInterestLoop face=" << inFace.getId() <<
                " interest=" << interest.getName());
return;
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

  // send Interest
  outFace.sendInterest(*interest);
  ++m_counters.getNOutInterests();
}

void
Forwarder::onOutgoingInterest(shared_ptr<pit::Entry> pitEntry, Face& outFace, std::string originMac,
                              std::string targetMac, int inFaceId, bool wantNewNonce)
{
  // bool debug = false;
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

  shared_ptr<Interest> interest2 = make_shared<Interest>(*interest);
  if (wantNewNonce) {
    static boost::random::uniform_int_distribution<uint32_t> dist;
    interest2->setNonce(dist(getGlobalRng()));
  }

  ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());


  // This constant is defined with a value of -1, which because size_t is an unsigned
  // integral type, it is the largest possible representable value for this type.
  std::size_t found = -1;
  std::string interest_macAddressPath = interest2->getMacAddressPath();

  // insert OutRecord
  pitEntry->insertOrUpdateOutRecord(outFace.shared_from_this(), *interest2);

  // TODO UNCOMMENT THIS LINE FOR TRYTRY
  // pitEntry->insertOrUpdateOriginMacRecord(originMac, *interest2);

  found = interest_macAddressPath.find(originMac);

  if(found == std::string::npos) {
    std::string in = " (in " + std::to_string(node->GetId()) + "/" + std::to_string(inFaceId) + ") ";
    std::string out = " (out " + std::to_string(node->GetId()) + "/" + std::to_string(outFace.getId()) + ") ";
    interest2->addMacAddressPath("\n\t     --> " +  in + originMac + out );
  }

  interest2->setInterestOriginMacAddress(originMac);
  interest2->setInterestTargetMacAddress(targetMac);
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
  // receive Data
  NFD_LOG_DEBUG("onIncomingData face=" << inFace.getId() << " data=" << data.getName());
  const_cast<Data&>(data).setIncomingFaceId(inFace.getId());
  ++m_counters.getNInDatas();

  // /localhost scope control
  bool isViolatingLocalhost = !inFace.isLocal() &&
                              LOCALHOST_NAME.isPrefixOf(data.getName());
  // std::cout << "isViolatingLocalhost: " << isViolatingLocalhost << std::endl;
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onIncomingData face=" << inFace.getId() <<
                  " data=" << data.getName() << " violates /localhost");
    // (drop)
    return;
  }
  // PIT match
  pit::DataMatchResult pitMatches = m_pit.findAllDataMatches(data);
  if (pitMatches.begin() == pitMatches.end()) {
      // goto Data unsolicited pipeline
      // this->onDataUnsolicited(inFace, data);
      return;
    }
  // all packages are dropped if they were not requested before


  ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());

  int numberOfNetDevices = node->GetNDevices();
  std::ostringstream addr[numberOfNetDevices];
  std::string currentMacAddresses[numberOfNetDevices];

  for(int index = 0; index < numberOfNetDevices; index++) {
	  addr[index] << node->GetDevice(index)->GetAddress();
	  currentMacAddresses[index] = addr[index].str().substr(6);
  }

  // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

  // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

	Name name;
	name = data.getName();
	std::ostringstream tmpName;
	tmpName << name;
	std::string localName = tmpName.str().substr(0,5);
	std::string fullDataName = tmpName.str().substr(0);

  // ************************ DROPPING OF DATA ****************************************************
  // **********************************************************************************************


	// SHOULD NEVER HAPPEN ANYWAY and has no effect on this scenario
//  if (node->GetId() == 7 && data.getDataOriginMacAddress() != "producer Mac") {
//  	  return;
//  }

  if(std::regex_match(data.getDataTargetMacAddress(), std::regex("([0-9a-fA-F]{2}[:-]){5}[0-9a-fA-F]{2}"))) {
	  if(data.getDataTargetMacAddress() == currentMacAddresses[0] || data.getDataTargetMacAddress() == currentMacAddresses[1]
			  || data.getDataTargetMacAddress() == currentMacAddresses[2] || data.getDataTargetMacAddress() == currentMacAddresses[3]) {
		  ns3::ndn::FibHelper::AddRoute(node, "/", inFace.getId(), 111, data.getDataOriginMacAddress());
	  } else {
		  ns3::ndn::FibHelper::AddRoute(node, "/", inFace.getId(), 222, data.getDataOriginMacAddress());
		  return;
	  }
	  // FOR NOW ELSE {} HAS NO EFFECT
  } else if (node->GetId() == 7 && data.getDataTargetMacAddress() == "lowerLayerOfProducer") {
	  //ns3::ndn::FibHelper::AddRoute(node, "/", inFace.getId(), 12, data.getDataOriginMacAddress());
  } else if (node->GetId() == 0 && (data.getDataTargetMacAddress() == "consumer" || false)) {

	  //ns3::ndn::FibHelper::AddRoute(node, "/", inFace.getId(), 12, data.getDataOriginMacAddress());
  } else {
	  // TODO: I THINK HERE IS A MISTAKE
//	  std::cout << "data has no target mac!!! how is that even possible?" << std::endl;
//	  std::cout << "data.targetMac: " << data.getDataTargetMacAddress() << std::endl;
//	  std::cout << "data.originMac: " << data.getDataOriginMacAddress() << std::endl;
//	  std::cout << "5556" << std::endl;
	  //return;
  }

  string initial_mac_target_data = data.getDataTargetMacAddress();

  shared_ptr<Data> dataCopyWithoutPacket = make_shared<Data>(data);
  dataCopyWithoutPacket->removeTag<ns3::ndn::Ns3PacketTag>();


  std::set<shared_ptr<Face>> pendingDownstreams;
  // foreach PitEntry


  // TODO probably only valid when one pitEntry in pitMatches
  std::list<std::string> dataTargetMac;


  // *************** TAKE PIT ENTRY AND EXTRACT NEEDED FACES ***********************************
  // *******************************************************************************************
  for (const shared_ptr<pit::Entry>& pitEntry : pitMatches) {
	  dataTargetMac = pitEntry->getOriginMacRecords();
    // cancel unsatisfy & straggler timer
     this->cancelUnsatisfyAndStragglerTimer(pitEntry);

    // remember pending downstreams
    const pit::InRecordCollection& inRecords = pitEntry->getInRecords();

    for (pit::InRecordCollection::const_iterator it = inRecords.begin();
                                                 it != inRecords.end(); ++it) {
      if (it->getExpiry() > time::steady_clock::now()) {

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
   // std::cout << " DATA NAME:--" << data.getName() <<"--"<< std::endl;
//    if (data.getName() == "/test/%FE%00") { }else{
    pitEntry->deleteInRecords();
    // TODO: understand why pitEntry->deleteOutRecord() needs inFace... is it possible to have
    // per pitEntry several outRecords that are not deleted while all inRecords do get deleted???
    pitEntry->deleteOutRecord(inFace);

    // set PIT straggler timer
    // The purpose of the straggler timer is to keep PIT entry alive for a short
    // period of time in order to facilitate duplicate Interest detection and to
    // collect data plane measurements.
    this->setStragglerTimer(pitEntry, true, data.getFreshnessPeriod());
 // }
  }

  // foreach pending downstream
  for (std::set<shared_ptr<Face> >::iterator it = pendingDownstreams.begin();
		  	  	  it != pendingDownstreams.end(); ++it) {
	  shared_ptr<Face> pendingDownstream = *it;

	  // ********************** some logic how to add more faces to the downstream *************************************
	  // ***************************************************************************************************************
			  for (const shared_ptr<pit::Entry>& pitEntry : pitMatches) {
				  // TODO: gives back the wrong mac address (same as target...
				  // std::string targetMac = pitEntry->getInterest().getInterestOriginMacAddress();
				  std::list<std::string> targetMacs = pitEntry->getOriginMacRecords();
				  std::list<std::string>::const_iterator stringIterator;

				  shared_ptr<Data> dataWithNewTargetMac = make_shared<Data>(data);

				  const pit::OutRecordCollection& outRecords = pitEntry->getOutRecords();

				  // TODO I THINK THIS IS WRONG !!!!
				  for (pit::OutRecordCollection::const_iterator it = outRecords.begin();
																 it != outRecords.end(); ++it) {
					  if((node->GetId() % 2) == 0) {
						  if(it->getFace()->getId() % 2 == 0) {
							  // TODO: fix logic
							  for(stringIterator = targetMacs.begin(); stringIterator != targetMacs.end(); stringIterator++) {
								  dataWithNewTargetMac->setDataTargetMacAddress(*stringIterator);
								  this->onOutgoingData(*dataWithNewTargetMac, *it->getFace(),*stringIterator, initial_mac_target_data);
								  break;
							  }

						  }
					  }

					  if((node->GetId() % 2) == 1) {
						  if(it->getFace()->getId() % 2 == 1) {
							  // TODO: fix logic
							  for(stringIterator = targetMacs.begin(); stringIterator != targetMacs.end(); stringIterator++) {
								  dataWithNewTargetMac->setDataTargetMacAddress(*stringIterator);
								  this->onOutgoingData(*dataWithNewTargetMac, *it->getFace(), *stringIterator, initial_mac_target_data);
								  break;
							  }
						  }
					  }
				  }
			  }
			  if(node->GetId() == 0 && data.getDataTargetMacAddress() == "consumer") {
				  std::cout << " 999 : probably never" << std::endl;
			  }

	  // ********************** some logic how to add more faces to the downstream *************************************
	  // ***************************************************************************************************************


	  // data cannot be send through the same face it was received on
	  if (pendingDownstream.get() == &inFace) {
		  continue;
	  }
	  std::list<std::string>::const_iterator stringIterator;

	  shared_ptr<Data> dataWithNewTargetMac = make_shared<Data>(data);
	  for(stringIterator = dataTargetMac.begin(); stringIterator != dataTargetMac.end(); stringIterator++) {
		  dataWithNewTargetMac->setDataTargetMacAddress(*stringIterator);
		  if (std::regex_match(*stringIterator, std::regex("([0-9a-fA-F]{2}[:-]){5}[0-9a-fA-F]{2}")) && node->GetId()!=0) {
			  this->onOutgoingData(*dataWithNewTargetMac, *pendingDownstream, *stringIterator, initial_mac_target_data );
			  break;
		  } else if (node->GetId() ==0 ) {
			  this->onOutgoingData(*dataWithNewTargetMac, *pendingDownstream, *stringIterator, initial_mac_target_data );
		  }
	  }
  }
}

void
Forwarder::onDataUnsolicited(Face& inFace, const Data& data)
{

  bool acceptToCache = inFace.isLocal();
  if (acceptToCache) {
	     // CS insert
    if (m_csFromNdnSim == nullptr)
      m_cs.insert(data, true);
    else

      m_csFromNdnSim->Add(data.shared_from_this());
  }
  std::cout<< "onDataUnsolicited face=" << inFace.getId() <<
  	                " data=" << data.getName() <<
  	                (acceptToCache ? " cached" : " not cached")<<std::endl;  // accept to cache?
  NFD_LOG_DEBUG("onDataUnsolicited face=" << inFace.getId() <<
                " data=" << data.getName() <<
                (acceptToCache ? " cached" : " not cached"));
}

void
Forwarder::onOutgoingData(const Data& data, Face& outFace)
{
	// bool debug = false;
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

	std::cout << "you really really shouldn't be here" << std::endl;

	// goto outgoing Data pipeline
	ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(ns3::Simulator::GetContext());

	// Since no TargetMac was given this data package we cannot know through which NetDevice it came from !!!!
	// That is a problem !!!

    int numberOfNetDevices = node->GetNDevices();
    std::ostringstream addr[numberOfNetDevices];
    std::string currentMacAddresses[numberOfNetDevices];

    for(int index = 0; index < numberOfNetDevices; index++) {
	    addr[index] << node->GetDevice(index)->GetAddress();
	    currentMacAddresses[index] = addr[index].str().substr(6);
    }

    std::string data_macRoute = data.getMacDataRoute();

	// check if this device's mac address has been added already to the interest. If yes the position of the
	// start position will be returned if the madAddress has not yet been added to the interest the function
	// will return std::string::npos
	std::size_t found = data_macRoute.find(currentMacAddresses[0]);

	// the only place where DataOriginMacAddress is set !!!!
	// TODO: WRONG CORRECT IT
	const_cast<Data&>(data).setDataOriginMacAddress(currentMacAddresses[0]);

	if(found == std::string::npos) {
		const_cast<Data&>(data).addMacDataRoute("\n\t       --> " + currentMacAddresses[0]);
	}

	// TODO traffic manager
	outFace.sendData(data);
	++m_counters.getNOutDatas();

}

void
Forwarder::onOutgoingData(const Data& data, Face& outFace, std::string macAddress, std::string initial_mac_target_data)
{
	// bool debug = false;
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

    int numberOfNetDevices = node->GetNDevices();
    std::ostringstream addr[numberOfNetDevices];
    std::string currentMacAddresses[numberOfNetDevices];

    for(int index = 0; index < numberOfNetDevices; index++) {
	    addr[index] << node->GetDevice(index)->GetAddress();
	    currentMacAddresses[index] = addr[index].str().substr(6);
    }

    std::string this_NetDevice_Mac = "";

	if(initial_mac_target_data == currentMacAddresses[0]){
		this_NetDevice_Mac = currentMacAddresses[0];
	} else if(initial_mac_target_data == currentMacAddresses[1]) {
		this_NetDevice_Mac = currentMacAddresses[1];
	} else if (initial_mac_target_data == currentMacAddresses[2]) {
		this_NetDevice_Mac = currentMacAddresses[2];
	} else if(initial_mac_target_data == currentMacAddresses[3]) {
		this_NetDevice_Mac = currentMacAddresses[3];
	} else {
		this_NetDevice_Mac = currentMacAddresses[producerMacSemaphore%4];
		producerMacSemaphore++;
	}


	std::string data_macRoute =  data.getMacDataRoute();

	if(std::regex_match(data.getDataTargetMacAddress(), std::regex("([0-9a-fA-F]{2}[:-]){5}[0-9a-fA-F]{2}"))) {
		if (node->GetId()!=7){
		  if( initial_mac_target_data != currentMacAddresses[0] && initial_mac_target_data != currentMacAddresses[1]
				  && initial_mac_target_data != currentMacAddresses[2] && initial_mac_target_data != currentMacAddresses[3]) {
			  return;
		  }
		}
	}

	// check if this device's mac address has been added already to the interest. If yes the position of the
	// start position will be returned if the madAddress has not yet been added to the interest the function
	// will return std::string::npos
	std::size_t found = data_macRoute.find(this_NetDevice_Mac);

	shared_ptr<Data> data2 = make_shared<Data>(data);
	data2->setDataOriginMacAddress(this_NetDevice_Mac);
	data2->setDataTargetMacAddress(macAddress);


	if(found == std::string::npos) {
		//const_cast<Data&>(data).addMacDataRoute("\n\t       --> " + this_NetDevice_Mac);
		data2->addMacDataRoute("\n\t       --> " + this_NetDevice_Mac);
	}
	// TODO traffic manager

	// send Data
	outFace.sendData(*data2);
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
