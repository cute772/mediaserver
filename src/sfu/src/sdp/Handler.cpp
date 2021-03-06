
#include "sdp/Handler.h"
#include "LoggerTag.h"
#include "base/error.h"
#include "sdp/ortc.h"
#include "sdp/signaler.h"
#include "sdp/Peer.h"
#include "sdp/Utils.h"
#include "base/uuid.h"
#include "sdp/Room.h"

using json = nlohmann::json;

namespace SdpParse {

    nlohmann::json Handler::_setupTransport(const json & sdpObject, const std::string & localDtlsRole) {

        // Get our local DTLS parameters.
        json dtlsParameters = Sdp::Utils::extractDtlsParameters(sdpObject);
        // Set our DTLS role.
        dtlsParameters["role"] = localDtlsRole;

        // Update the remote DTLS role in the SDP.
        remoteSdp->UpdateDtlsRole(
                localDtlsRole == "client" ? "server" : "client");

        return dtlsParameters;
    }

    void Handler::createSdp(const json& iceParameters, const json& iceCandidates, const json& dtlsParameters) {
        if (remoteSdp)
            delete remoteSdp;

        remoteSdp = new Sdp::RemoteSdp(iceParameters, iceCandidates, dtlsParameters, nullptr);
    }

    void Handler::transportCreate() {
        {

            json param = json::array();
            param.push_back("createWebRtcTransport");
            param.push_back(peer->participantID);

            json &trans = Settings::configuration.createWebRtcTransport;
            transportId = uuid4::uuid();

            SInfo << "transportId: " << transportId;

            if (!forceTcp) {
                trans["data"]["enableUdp"] = true;
                trans["data"]["preferUdp"] = true;
            }

            trans["data"]["listenIps"] = Settings::configuration.listenIps;
            raiseRequest(param, trans, [&](const json & ack_resp) {
                const json &ackdata = ack_resp["data"];
                createSdp(ackdata["iceParameters"], ackdata["iceCandidates"], ackdata["dtlsParameters"]);
            });

        }
        {

            json param = json::array();
            param.push_back("maxbitrate");
            param.push_back(peer->participantID);
            json &trans = Settings::configuration.maxbitrate;
            raiseRequest(param, trans, [](const json & ack_resp) {

            });
        }

    }

    void Handler::raiseRequest(json &param, json& trans, std::function<void (const json&) > func) {

        // trans["id"] = ++peer->reqId;
        trans["internal"]["transportId"] = transportId;
        trans["internal"]["routerId"] = peer->roomId;
        param.push_back(trans);

        signaler->request(classtype, param, true, func);

    }

    void Handler::transportConnect(const nlohmann::json& sdpObject, const std::string& localDtlsRole) {
        json dtlsParameters = _setupTransport(sdpObject, localDtlsRole);


        json param = json::array();
        param.push_back("transport.connect");
        param.push_back(peer->participantID);
        json &trans = Settings::configuration.transport_connect;
        //STrace << "peer->dtlsParameters " << dtlsParameters;
        trans["data"]["dtlsParameters"] = dtlsParameters;
        raiseRequest(param, trans, [&](const json & ack_resp) {
            //if(ack_resp)["accepted"] ) // arvind TBD later	
            transport_connect = true;

        });

    }

    Producers::Producers(Signaler *signaler, Peer *peer) : Handler(signaler, peer) {
        classtype = "Producers";
        transportCreate();
    }

    void Producers::GetAnswer(std::string& kind , json &sendingRtpParameters, std::string mid, std::string reuseMid, json &offerMediaObject) {


        sendingRtpParameters = peer->sendingRtpParametersByKind[kind];
        
        //SInfo << "offerMediaObject " << offerMediaObject.dump(4);
        
        sendingRtpParameters["mid"] = mid;
           
        //sendingRtpParameters["rtcp"]["cname"] = Sdp::Utils::getCname(offerMediaObject);
        sendingRtpParameters["rtcp"]["cname"] = peer->participantID;   // arvind risky code. I am changing cname and msid to particpant id

        // sendingRtpParameters["encodings"] = Sdp::Utils::getRtpEncodings(offerMediaObject);


        json encodings = json::array();

        if (offerMediaObject.find("rids") != offerMediaObject.end()) {
            int size = offerMediaObject["rids"].size();

            encodings.push_back({
                {"rid", "q"},
                {"scaleResolutionDownBy", 4.0}
            });

            encodings.push_back({
                {"rid", "h"},
                {"scaleResolutionDownBy", 2.0}
            });

            if (size > 2)
                encodings.push_back({
                    {"rid", "f"}
                });
        }


        if (!encodings.size()) {

            sendingRtpParameters["encodings"] = Sdp::Utils::getRtpEncodings(offerMediaObject);
        }            // Set RTP encodings by parsing the SDP offer and complete them with given
            // one if just a single encoding has been given.
        else if (encodings.size() == 1) {
            auto newEncodings = Sdp::Utils::getRtpEncodings(offerMediaObject);
            //  Object.assign(newEncodings[0], encodings[0]);  // TBD // this is for FID most probabily we need to check
            sendingRtpParameters["encodings"] = newEncodings;
        }            // Otherwise if more than 1 encoding are given use them verbatim.
        else {
            sendingRtpParameters["encodings"] = encodings;
        }


        // If VP8 and there is effective simulcast, add scalabilityMode to each encoding.
        auto mimeType = sendingRtpParameters["codecs"][0]["mimeType"].get<std::string>();

        std::transform(mimeType.begin(), mimeType.end(), mimeType.begin(), ::tolower);

        if (sendingRtpParameters["encodings"].size() > 1 && (mimeType == "video/vp8" || mimeType == "video/h264")) {
            for (auto& encoding : sendingRtpParameters["encodings"]) {
                encoding["scalabilityMode"] = "S1T3";
            }
        }

        // STrace << "sendingRtpParameters "  <<  sendingRtpParameters ;

        json *codecOptions = nullptr;

        //  _setupTransport(peer->sdpObject, "server");

        remoteSdp->Send(
                offerMediaObject,
                reuseMid,
                sendingRtpParameters,
                peer->sendingRemoteRtpParametersByKind[kind],
                codecOptions);

    }
    void Producers::close_producer( const std::string &producerid)
    {
        json param = json::array();
        param.push_back("producer.close");
        param.push_back(peer->participantID);
        json &trans = Settings::configuration.producer_getStats;
        trans["internal"]["producerId"] = producerid;//uuid4::uuid();
        trans["method"] = "producer.close";
        raiseRequest(param, trans, [&](const json & ack_resp) {
        //SInfo <<  " Deleted " << ack_resp;
        });
    }
    void Producers::runit(std::function<void (const std::string &) > cbAns) {


        if(!transport_connect)
        transportConnect(peer->sdpObject, "server");

        int sizeofMid = peer->sdpObject["media"].size();
        
//        if(sizeofMid == mapProdMid.size()) 
//        {
//            
//            for (int i = 0; i < sizeofMid ; ++i) {
//
//              json& offerMediaObject = peer->sdpObject["media"][i];
//              std::string producerId = mapProdMid[i];  
//              std::string direction = offerMediaObject["direction"].get<std::string>();
//              if( direction == "inactive" &&  (mapProducer.find(producerId) !=  mapProducer.end()))
//              {
//                  std::string mid = offerMediaObject["mid"].get<std::string>();
//                  remoteSdp->CloseMediaSection(mid);
//                  std::string trackid = Sdp::Utils::extractTrackID(offerMediaObject);
//                  SInfo << "ProducerClose Mid " << mid << " Producer id "  << trackid;
//                          
//                  if( producerId != trackid)
//                  {
//                      SError<< "Producer Close Mid Error " << mid << " Producer id "  << trackid;
//                      exit(-1);
//                  }
//                  
//                  delete mapProducer[trackid];
//                  mapProducer.erase(trackid);
//                  
//                  close_producer(trackid);
//              
//              }
//                 
//            }
//            
//            auto answer = remoteSdp->GetSdp();
//            
//            //SInfo << " ans "  << answer;
//            cbAns(answer);
//                  
//            return;
//        }

        for (int i = 0; i < sizeofMid ; ++i) {
            
            json& offerMediaObject = peer->sdpObject["media"][i];

            std::string kind = offerMediaObject["type"].get<std::string>();

            std::string trackid= Sdp::Utils::extractTrackID(offerMediaObject);

            //SInfo << "sendingRtpParameters " << sendingRtpParameters.dump(4);

            auto midIt = offerMediaObject.find("mid");

            if (midIt == offerMediaObject.end()) {
            SError << "Found no mid in SDP";
            throw "Found no mid in SDP";
            }
            
            std::string smid = (*midIt).get<std::string>();
            int mid = std::stoi(smid);
    
            //////////////////////////////////////
            std::string producerId = mapProdMid[mid];  
            std::string direction = offerMediaObject["direction"].get<std::string>();
            if( direction == "inactive" &&  (mapProducer.find(producerId) !=  mapProducer.end()))
            {
                //std::string mid = offerMediaObject["mid"].get<std::string>();
                remoteSdp->CloseMediaSection(smid);
                std::string trackid = Sdp::Utils::extractTrackID(offerMediaObject);
                SInfo << "ProducerClose Mid " << mid << " Producer id "  << trackid;

                if( producerId != trackid)
                {
                    SError<< "Producer Close Mid Error " << mid << " Producer id "  << trackid;
                    exit(-1);
                }

                delete mapProducer[trackid];
                mapProducer.erase(trackid);
                mapProdMid.erase(mid);
                
                json param = json::array();
                param.push_back("producer.close");
                param.push_back(peer->participantID);
                json &trans = Settings::configuration.producer_getStats;
                trans["internal"]["producerId"] = trackid;//uuid4::uuid();
                trans["method"] = "producer.close";
                raiseRequest(param, trans, [&](const json & ack_resp) {
                    
                     if (sizeofMid == i+1) {
                        auto answer = remoteSdp->GetSdp();
                                cbAns(answer);
                    }
                
                });

            }
            //////////////////////////////////////////
        
           if( (mapProducer.find(producerId) ==  mapProducer.end()))
           {

                json sendingRtpParameters;
                Producer *p = new Producer();

             //   Sdp::RemoteSdp::MediaSectionIdx mediaSectionIdx = remoteSdp->GetNextMediaSectionIdx();

                GetAnswer(kind, sendingRtpParameters, *midIt, "", offerMediaObject);

                json param = json::array();
                param.push_back("transport.produce");
                param.push_back(peer->participantID);
                json &trans = Settings::configuration.transport_produce;

                trans["internal"]["producerId"] = trackid;//uuid4::uuid();


                /////////////////////
                if (constructor_name != "PipeTransport") {
                    // If CNAME is given and we don't have yet a CNAME for Producers in this
                    // Transport, take it.
                    if (cnameForProducers.empty() && sendingRtpParameters.find("rtcp") != sendingRtpParameters.end() && sendingRtpParameters["rtcp"].find("cname") != sendingRtpParameters["rtcp"].end()) {
                        cnameForProducers = sendingRtpParameters["rtcp"]["cname"];
                    }
                    // Otherwise if we don't have yet a CNAME for Producers and the RTP parameters
                    // do not include CNAME, create a random one.
                    if (cnameForProducers.empty()) {
                        cnameForProducers = base::util::randomString(8);
                    }
                    // Override Producer's CNAME.

                    if (sendingRtpParameters.find("rtcp") == sendingRtpParameters.end()) {
                        sendingRtpParameters["rtcp"] = json::object();
                    }

                    sendingRtpParameters["rtcp"]["cname"] = cnameForProducers;
                }

                ///////////////////////



                // This may throw.
                auto rtpMapping = SdpParse::ortc::getProducerRtpParametersMapping(sendingRtpParameters, Settings::configuration.routerCapabilities);

                auto consumableRtpParameters = SdpParse::ortc::getConsumableRtpParameters(kind, sendingRtpParameters, Settings::configuration.routerCapabilities, rtpMapping);


                //SInfo << "consumableRtpParameters " << consumableRtpParameters.dump(4);
                // STrace << "rtpMapping " << rtpMapping.dump(4);

                json data = {
                    {"kind", kind},
                    {"paused", false},
                    {"rtpMapping", rtpMapping},
                    {"rtpParameters", sendingRtpParameters},
                };

                trans["data"] = data;

                raiseRequest(param, trans, [&](const json & ack_resp) {

                    p->producer = {
                        { "id", trans["internal"]["producerId"]},
                        {"kind", kind},
                        {"recvRtpCapabilities", peer->GetRtpCapabilities()}, //{"rtpParameters", sendingRtpParameters},
                        {"type", ack_resp["data"]["type"]},
                        { "consumableRtpParameters", consumableRtpParameters}
                    };

                    // SInfo << "Final Producer " << p->producer.dump(4);
                    mapProducer[p->producer["id"]] = p;
                    mapProdMid[ mid] = p->producer["id"];
                    SInfo << "mid : " <<  mid << " mapProducer " << p->producer["id"] << " kind " << p->producer["kind"];

                    if (sizeofMid == i+1) {
                        auto answer = remoteSdp->GetSdp();
                                cbAns(answer);
                    }


                });
        }


       

        }
    }

    void Producers::producer_getStats(const std::string& producerId) {

        
        if( mapProducer.find(producerId) == mapProducer.end() )
        {
            SError << "ProdStats: Could not find consumer  " << producerId << " for particpant " << peer->participantID;
            return ;
        }

        auto &prod = mapProducer[producerId];

        json &producer = prod->producer;

        json param = json::array();
        param.push_back("producer.getStats");
        param.push_back(peer->participantID);
        json &trans = Settings::configuration.producer_getStats;

        trans["internal"]["producerId"] = producer["id"];
        raiseRequest(param, trans, [&](const json & ack_resp) {

        //SInfo << "stats: " << ack_resp.dump(4);

        json m;
        m["type"] = "prodstats";
        m["desc"] = ack_resp;
        m["to"] = peer->participantID;
        signaler->postAppMessage(m);

        });

        
    }

    Producers::~Producers() {
        SInfo << "~Producers()";

        for (auto &prod : mapProducer) {
          //  close_producer(prod.second->producer["producerId"] );
            delete prod.second;
        }

        mapProducer.clear();
        mapProdMid.clear();
    }

    void Producers::rtpObserver_addProducer(bool flag) {

        for (auto &prod : mapProducer) {

            json &producer = prod.second->producer;
            
            if (producer["kind"] == "audio") {


                json param = json::array();
                if (flag)
                    param.push_back("rtpObserver_addProducer");
                else
                    param.push_back("rtpObserver_removeProducer");

                param.push_back(peer->participantID);
                json &trans = Settings::configuration.rtpObserver_addProducer;
                trans["internal"]["producerId"] = producer["id"];

                if (flag)
                    trans["method"] = "rtpObserver.addProducer";
                else
                    trans["method"] = "rtpObserver.removeProducer";


                if (flag) // && ok ack  TBD
                {
                    signaler->mapNotification[trans["internal"]["rtpObserverId"]][ producer["id"]] = peer->participantID;
                }
                raiseRequest(param, trans, [&](const json & ack_resp) {

                });

            }

        }
    }

    void Producers::resume(const std::string& producerId, bool pause) {

           if( mapProducer.find(producerId) == mapProducer.end() )
           {
               SError << "Could not find Producer  " << producerId << " for particpant " << peer->participantID;
               return ;
           }
           
            auto &prods = mapProducer[producerId];

            json &producer = prods->producer;
 
            json &trans = Settings::configuration.pause_resume;
            json param = json::array();
            if (pause) {
                param.push_back("producer.pause");
                trans["method"] = "producer.pause";
            } else {
                param.push_back("producer.resume");
                trans["method"] = "producer.resume";
            }
            param.push_back(peer->participantID);

            trans["internal"]["producerId"] = producer["id"];
          
            raiseRequest(param, trans, [&](const json & ack_resp) {

            });

    }
    /*************************************************************************************************************
        Consumer starts
     *************************************************************************************************************/
    Consumers::Consumers(Signaler *signaler, Peer * peer) : Handler(signaler, peer) {
        classtype = "Consumers";
        transportCreate();
    }


    //    std::string Consumers::GetOffer(const std::string& id, const std::string&  mid, const std::string& kind, const json& rtpParameters) {
    //
    //        // mid is optional, check whether it exists and is a non empty string.
    ////        auto midIt = rtpParameters.find("mid");
    ////        if (midIt != rtpParameters.end() && (midIt->is_string() && !midIt->get<std::string>().empty()))
    ////            localId = midIt->get<std::string>();
    ////        else
    ////            localId = std::to_string(mid++);
    //        
    //       
    //
    //        auto& cname = rtpParameters["rtcp"]["cname"];
    //
    //        this->remoteSdp->Receive(mid, kind, rtpParameters, cname, id);
    //
    //        auto offer = this->remoteSdp->GetSdp();
    //
    //        STrace << "offer: " << offer;
    //        return offer;
    //    }

    void Consumers::resume(const std::string& consumerId, bool pause) {

           if( mapConsumer.find(consumerId) == mapConsumer.end() )
           {
               SError << "Could not find consumer  " << consumerId << " for particpant " << peer->participantID;
               return ;
           }
           
            auto &cons = mapConsumer[consumerId];

            json &consumer = cons->consumer;

            json &trans = Settings::configuration.pause_resume;
            json param = json::array();
            if (pause) {
                param.push_back("consumer.pause");
                trans["method"] = "consumer.pause";
            } else {
                param.push_back("consumer.resume");
                trans["method"] = "consumer.resume";
            }
            param.push_back(peer->participantID);

            trans["internal"]["producerId"] = consumer["producerId"];
            trans["internal"]["consumerId"] = consumer["id"];

            raiseRequest(param, trans, [&](const json & ack_resp) {

            });

    }

    /**
     * Load andwer SDP.
     */
    void Consumers::loadAnswer(std::string sdp) {
        ////////////////////////
        json ansdpObject = sdptransform::parse(sdp);
        //SDebug << "answer " << ansdpObject.dump(4);
        //  _setupTransport(ansdpObject, "client");

        transportConnect(ansdpObject, "client");

    }
    
//    void Consumers::sendOffer(const std::string& id, const std::string&  mid, const std::string& kind, const json& rtpParameters , const std::string& partID , const std::string& remotePartID) {
            
//        int iMid = remoteSdp->MediaSectionSize();
//        std::string localId;
//        localId = std::to_string(iMid);
//        mapConMid[iMid] = id;
//        auto& cname = rtpParameters["rtcp"]["cname"];
//
//
//       // SInfo <<  localId << " Sendffer  for consumer" << id << " kind " << kind;
//
//        this->remoteSdp->Receive(localId, kind, rtpParameters, cname, id);
//
//        if( remoteSdp->MediaSectionSize() == nodevice )
//        {
//            SInfo << "mid " <<  localId << " Sendffer  for consumer" << id << " nodevice " <<  (int) nodevice << " remoteSdp->MediaSectionSize() " << remoteSdp->MediaSectionSize();
//
//            auto offer = this->remoteSdp->GetSdp();
//           // SInfo << "offer: " << offer;
//            signaler->sendSDP("offer", offer, partID, remotePartID);
//        }
           

 //    }
    
//    void Consumers::onUnSubscribe( const std::string& producerPeer)
//    {
//        
//        for( int mid = mapProdDevs[producerPeer].first ; mid <= mapProdDevs[producerPeer].second ;  ++mid)
//        {
//            std::string conumserid = mapConMid[mid ];
//            if( mapConsumer.find(conumserid) != mapConsumer.end()    )
//            {
//                SInfo << "Logout: Close Mid: " <<  mid << " Conumer id: " << conumserid ; 
//                remoteSdp->CloseMediaSection(std::to_string(mid));
//                delete mapConsumer[conumserid];
//                mapConsumer.erase(conumserid);
//            }
//        }
//        
//        auto offer = this->remoteSdp->GetSdp();
//           // SInfo << "offer: " << offer;
//        signaler->sendSDP("offer", offer, peer->participantID, producerPeer);
//            
//    }

    void Consumers::close_consumer(const std::string& producerid, const std::string& conumserid )
    {
         json param = json::array();
        param.push_back("consumer.close");
        param.push_back(peer->participantID);
        json &trans = Settings::configuration.consumer_getStats;
        trans["method"]="consumer.close";
        trans["internal"]["producerId"] = producerid ;
        trans["internal"]["consumerId"] = conumserid;

        raiseRequest(param, trans, [&](const json & ack_resp) {

        });
    }
    void Consumers::runit(std::vector < Peer *> vecProdPeer) {
        
       
        bool lastDevice = false;
        int noProd=0 ;      
        for ( auto & prodpeer : vecProdPeer)
        {
         
          ++noProd;

            
//        if(mid < 0)
//        {
//            
//            for (auto& prodMid : producers->mapProdMid) {
//               if( producers->mapProducer.find(prodMid.second) == producers->mapProducer.end() )
//               {
//                   int mid = mapProdDevs[producers->peer->participantID].first  + prodMid.first;
//                   std::string conumserid = mapConMid[mid ];
//                   if( mapConsumer.find(conumserid) !=  mapConsumer.end()  )
//                   {
//                        SInfo << "Close Mid: " <<  mid << " Conumer id: " << conumserid << " Producer ID: " << prodMid.second; 
//                        remoteSdp->CloseMediaSection(std::to_string(mid));
//                        
//                        if( prodMid.second != mapConsumer[conumserid]->consumer["producerId"] )
//                        {
//                            SError << "Not a vaid state. Some thing is terrible wrong " << prodMid.second  << " " <<  mapConsumer[conumserid]->consumer["producerId"];
//                            exit(-1);
//                        }
//                        
//                        delete mapConsumer[conumserid];
//                        mapConsumer.erase(conumserid);
//                        
//                        ////////////////////////////
//                       
//                      //  close_consumer(prodMid.second , conumserid );  // no need since porducer already closed, it will close dependent consumers
//
//                        ////////////////////////////
//                        
//                   }
//               }
//
//          }
//            
//            auto offer = this->remoteSdp->GetSdp();
//           // SInfo << "offer: " << offer;
//            signaler->sendSDP("offer", offer, peer->participantID, producers->peer->participantID);
//            
//            return ;
//        }
        int nodevice = 0;
        if( prodpeer->producers)
        for (auto& prodMid : prodpeer->producers->mapProdMid) {

            if( prodpeer->producers->mapProducer.find(prodMid.second) == prodpeer->producers->mapProducer.end() )
              continue;   
            
            ++nodevice;
            if( nodevice == prodpeer->producers->mapProdMid.size() )
            {
                if( noProd == vecProdPeer.size())
                {
                    lastDevice = true;
                }
            }
            
            json &producer = prodpeer->producers->mapProducer[prodMid.second]->producer;
            std::vector < Producers::MapProCon > &vecProCon = prodpeer->producers->mapProducer[prodMid.second]->vecProCon;

            json param = json::array();
            param.push_back("transport.consume");
            param.push_back(peer->participantID);
            json trans = Settings::configuration.transport_consume;

            trans["internal"]["producerId"] = producer["id"];
            trans["internal"]["consumerId"] = uuid4::uuid();



            //json rtpParameters = SdpParse::ortc::getConsumerRtpParameters(producer["consumableRtpParameters"], (json&) peer->GetRtpCapabilities());
            json rtpParameters = SdpParse::ortc::getConsumerRtpParameters(producer["consumableRtpParameters"], producer["recvRtpCapabilities"]);
            //  const internal = Object.assign(Object.assign({}, this._internal), { consumerId: v4_1.default(), producerId });

            //STrace << "getConsumerRtpParameters " << rtpParameters.dump(4);

            //bool paused = producer["kind"] == "video" ? true : false;

            json reqData = {
                {"kind", producer["kind"]},
                {"rtpParameters", rtpParameters},
                {"type", producer["type"]},
                {"consumableRtpEncodings", producer["consumableRtpParameters"]["encodings"]},
                {"paused", false}
            };

            trans["data"] = reqData;

           
            raiseRequest(param, trans, [&](const json & ack_resp) {
               // SInfo << ack_resp.dump(4);
                const json &ackdata = ack_resp["data"];
               // SInfo << localId << " GetOffer " << trans["internal"]["consumerId"] << " kind " << reqData["kind"];
                Consumer *c = new Consumer();
                c->consumer = {

                    {"id", trans["internal"]["consumerId"]},
                    {"producerId", producer["id"]},
                    {"kind", trans["data"]["kind"]},
                    {"rtpParameters", trans["data"]["rtpParameters"]},
                    {"type", trans["data"]["type"]},
                    {"paused", ackdata["paused"]},
                    {"producerPaused", ackdata["producerPaused"]},
                    {"score", ackdata["score"]},
                    {"preferredLayers", ackdata["preferredLayers"]}

                };

               
                //SInfo <<  localId << " GetOffer " << c->consumer["id"] << " kind " << c->consumer["kind"];
                mapConsumer[ c->consumer["id"]] = c;

                auto& cname = c->consumer["rtpParameters"]["rtcp"]["cname"];
                
                int iMid = remoteSdp->MediaSectionSize();
                std::string localId;
                localId = std::to_string(iMid);
                mapConMid[iMid] = c->consumer["id"];
                
                Producers::MapProCon mapProCon;
                mapProCon.cons = this;
                mapProCon.consumerId = c->consumer["id"];
                
                vecProCon.push_back(mapProCon);
            
                this->remoteSdp->Receive(localId, c->consumer["kind"], c->consumer["rtpParameters"], cname, c->consumer["id"]);
                
                if( lastDevice )
                {
                    SInfo << "mid " <<  localId << " Sendffer  for consumer" << c->consumer["id"] <<  " remoteSdp->MediaSectionSize() " << remoteSdp->MediaSectionSize();

                    auto offer = this->remoteSdp->GetSdp();
                   // SInfo << "offer: " << offer;
                    signaler->sendSDP("offer", offer, peer->participantID, prodpeer->producers->peer->participantID);
                }
               
                // sendOffer(c->consumer["id"], "" , c->consumer["kind"], c->consumer["rtpParameters"], peer->participantID, producers->peer->participantID );


            });

            ////////////////////////////////////////////////////////////////////////////////

//            if (!_probatorConsumerCreated && producer["kind"] == "video") {
//                _probatorConsumerCreated = true;
//                {
//                    json probatorRtpParameters = ortc::generateProbatorRtpParameters(rtpParameters);
//
//                    ///////////////
//
//                    json param = json::array();
//                    param.push_back("transport.consume");
//                    param.push_back(peer->participantID);
//                    json &trans = Settings::configuration.transport_consume;
//
//                    trans["internal"]["producerId"] = producer["id"];
//                    trans["internal"]["consumerId"] = "probator";
//
//
//                    json reqData = {
//                        {"kind", producer["kind"]},
//                        {"rtpParameters", probatorRtpParameters},
//                        {"type", producer["type"]},
//                        {"consumableRtpEncodings", producer["consumableRtpParameters"]["encodings"]},
//                        {"paused", false}
//                    };
//
//                    trans["data"] = reqData;
//                    raiseRequest(param, trans, [this, trans, reqData, producer](const json & ack_resp) {
//
//                        const json &ackdata = ack_resp["data"];
//
//                        Consumer *c = new Consumer();
//                        c->consumer = {
//
//                            {"id", trans["internal"]["consumerId"]},
//                            {"producerId", producer["id"]},
//                            {"kind", trans["data"]["kind"]},
//                            {"rtpParameters", trans["data"]["rtpParameters"]},
//                            {"type", trans["data"]["type"]},
//                            {"paused", ackdata["paused"]},
//                            {"producerPaused", ackdata["producerPaused"]},
//                            {"score", ackdata["score"]},
//                            {"preferredLayers", ackdata["preferredLayers"]}
//
//                        };
//
//
//                        //  GetOffer(c->consumer["id"], "probator" , c->consumer["kind"], c->consumer["rtpParameters"]);
//                        //SInfo <<  localId << " GetOffer " << trans["internal"]["consumerId"]<< " kind " << reqData["kind"];
//                        mapConsumer[ c->consumer["id"]] = c;
//
//                        auto& cname = c->consumer["rtpParameters"]["rtcp"]["cname"];
////                        remoteSdp->Receive("probator", c->consumer["kind"], c->consumer["rtpParameters"], cname, c->consumer["id"]);
////                        if (sizeofMid == remoteSdp->MediaSectionSize()) {
////                            auto offer = this->remoteSdp->GetSdp();
////                                    cboffer(offer);
////                        }
//
//                        //////////
//                    });
//
//
//                }
//
            }

        }

    }

    void Consumers::consumer_getStats(const std::string& consumerId ) {
        
        if( mapConsumer.find(consumerId) == mapConsumer.end() )
        {
            SError << "GetStats: Could not find consumer  " << consumerId << " for particpant " << peer->participantID;
            return ;
        }

         auto &cons = mapConsumer[consumerId];

           
        json &consumer = cons->consumer;


        json param = json::array();
        param.push_back("consumer_getStats");
        param.push_back(peer->participantID);
        json &trans = Settings::configuration.consumer_getStats;

        trans["internal"]["producerId"] = consumer["producerId"];
        trans["internal"]["consumerId"] = consumer["id"];

        raiseRequest(param, trans, [&](const json & ack_resp) {

        // SInfo << "stats: " << ack_resp.dump(4);

        json m;
        m["type"] = "constats";
        m["desc"] = ack_resp;
        m["to"] = peer->participantID;
        signaler->postAppMessage(m);


         });


    }

    Consumers::~Consumers() {
        for (auto &cons : mapConsumer) {
            
            close_consumer(cons.second->consumer["producerId"], cons.second->consumer["id"] );
            delete cons.second;
        }
        mapConsumer.clear();
        mapConMid.clear();
    }

    void Consumers::setPreferredLayers(json &layer) {

        std::string consumerId = layer["id"].get<std::string>();
        
        if( mapConsumer.find(consumerId) == mapConsumer.end() )
        {
            SError << "setPreferredLayers: Could not find consumer  " << consumerId << " for particpant " << peer->participantID;
            return ;
        }

        auto &cons = mapConsumer[consumerId];
        json &consumer = cons->consumer;

        //SInfo << " setPreferredLayers conusmer: " << consumer.dump(4);

        if (consumer["kind"] == "video" && consumer["type"] == "simulcast") {
            json param = json::array();
            param.push_back("consumer_setPreferredLayers");
            param.push_back(peer->participantID);
            json &trans = Settings::configuration.consumer_setPreferredLayers;
            trans["internal"]["producerId"] = consumer["producerId"];
            trans["internal"]["consumerId"] = consumer["id"];
            trans["data"] = layer["data"];
            raiseRequest(param, trans, [](const json & ack_resp) {
                SInfo << ack_resp.dump(4);
            });
        }

    }
   


} // namespace SdpParse
