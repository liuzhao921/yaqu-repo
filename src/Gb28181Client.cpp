#include "Gb28181Client.h"
#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <osip2/osip_sdp.h>
#include <random>
#include <cstdio> // For std::system

// For generating a unique SN (sequence number)
std::atomic<int> sn_counter(0);

// Starting RTP port for dynamic allocation
const int RTP_PORT_START = 10000;
const int RTP_PORT_END = 20000;

Gb28181Client::Gb28181Client(const std::string& serverIp, int serverPort, const std::string& deviceId, const std::string& realm, const std::string& password)
    : serverIp_(serverIp), serverPort_(serverPort), deviceId_(deviceId), realm_(realm), password_(password), 
      running_(false), context_(nullptr), registerId_(-1), nextRtpPort_(RTP_PORT_START) {
    
    context_ = eXosip_malloc();
    if (eXosip_init(context_) != 0) {
        std::cerr << "Failed to initialize eXosip context" << std::endl;
    }
}

Gb28181Client::~Gb28181Client() {
    stop();
    if (context_) {
        eXosip_quit(context_);
    }
}

void Gb28181Client::start() {
    if (!running_ && context_) {
        if (eXosip_listen_addr(context_, IPPROTO_UDP, nullptr, 5060, AF_INET, 0) != 0) {
            std::cerr << "Failed to listen on port 5060" << std::endl;
            return;
        }

        running_ = true;

        std::string from = "sip:" + deviceId_ + "@" + realm_;
        std::string proxy = "sip:" + serverIp_ + ":" + std::to_string(serverPort_);
        osip_message_t *reg = nullptr;

        registerId_ = eXosip_register_build_initial_register(context_, from.c_str(), proxy.c_str(), nullptr, 3600, &reg);
        if (registerId_ > 0) {
            eXosip_add_authentication_info(context_, deviceId_.c_str(), deviceId_.c_str(), password_.c_str(), nullptr, realm_.c_str());
            eXosip_register_send_register(context_, registerId_, reg);
        }

        eventThread_ = std::thread(&Gb28181Client::eventLoop, this);
        keepAliveThread_ = std::thread(&Gb28181Client::keepAliveLoop, this);
        std::cout << "GB28181 Client started with eXosip2." << std::endl;
    }
}

void Gb28181Client::stop() {
    if (running_) {
        running_ = false;
        if (eventThread_.joinable()) {
            eventThread_.join();
        }
        if (keepAliveThread_.joinable()) {
            keepAliveThread_.join();
        }

        // Stop all active RTP sessions
        std::lock_guard<std::mutex> lock(rtpSessionsMutex_);
        for (auto const& [callId, session] : rtpSessions_) {
            session.stop(); // This will join the thread
        }
        rtpSessions_.clear();

        std::cout << "GB28181 Client stopped." << std::endl;
    }
}

void Gb28181Client::eventLoop() {
    while (running_) {
        eXosip_event_t *ev = eXosip_event_wait(context_, 0, 50);
        if (!ev) {
            continue;
        }

        switch (ev->type) {
            case EXOSIP_REGISTRATION_SUCCESS:
                std::cout << "GB28181: Registration successful for " << deviceId_ << std::endl;
                break;
            case EXOSIP_REGISTRATION_FAILURE:
                std::cerr << "GB28181: Registration failed for " << deviceId_ << std::endl;
                break;
            case EXOSIP_MESSAGE_NEW:
                std::cout << "GB28181: New MESSAGE received" << std::endl;
                handleMessage(ev);
                break;
            case EXOSIP_MESSAGE_ANSWERED:
                handleMessageAnswer(ev);
                break;
            case EXOSIP_CALL_INVITE:
                std::cout << "GB28181: New INVITE received (RealPlay request)" << std::endl;
                handleInvite(ev);
                break;
            case EXOSIP_CALL_ACK:
                std::cout << "GB28181: Received ACK for call ID: " << ev->cid << std::endl;
                handleAck(ev);
                break;
            case EXOSIP_CALL_CLOSED:
                std::cout << "GB28181: Call closed for call ID: " << ev->cid << std::endl;
                handleBye(ev);
                break;
            default:
                std::cout << "GB28181: Received event type: " << ev->type << std::endl;
                break;
        }
        eXosip_event_free(ev);
    }
}

void Gb28181Client::keepAliveLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(60)); // Send keep-alive every 60 seconds
        if (!running_) break;

        std::string from = "sip:" + deviceId_ + "@" + realm_;
        std::string to = "sip:" + serverIp_ + ":" + std::to_string(serverPort_);
        osip_message_t *keepalive_msg = nullptr;
        eXosip_message_build_request(context_, &keepalive_msg, "MESSAGE", to.c_str(), from.c_str(), nullptr);

        if (keepalive_msg) {
            std::string keepalive_xml = buildKeepAliveMessage();
            osip_message_set_content_type(keepalive_msg, "Application/MANSCDP+xml");
            osip_message_set_body(keepalive_msg, keepalive_xml.c_str(), keepalive_xml.length());
            eXosip_message_send_request(context_, keepalive_msg);
            std::cout << "Sent Keep-alive message." << std::endl;
        }
    }
}

void Gb28181Client::handleMessage(eXosip_event_t* ev) {
    if (!ev || !ev->request) {
        return;
    }

    osip_message_t *request = ev->request;
    osip_body_t *body = nullptr;
    osip_message_get_body(request, 0, &body);

    if (body && body->body) {
        std::string xmlContent(body->body);
        std::cout << "Received XML: " << xmlContent << std::endl;

        xmlDocPtr doc = xmlReadMemory(xmlContent.c_str(), xmlContent.length(), "noname.xml", nullptr, 0);
        if (doc) {
            xmlNodePtr root_element = xmlDocGetRootElement(doc);
            if (root_element) {
                std::string cmdType, sn, deviceId, ptzCmd;
                xmlNodePtr current_node = root_element->children;
                while (current_node) {
                    if (xmlStrcmp(current_node->name, (const xmlChar *) "CmdType") == 0) {
                        cmdType = (char*)xmlNodeGetContent(current_node);
                    } else if (xmlStrcmp(current_node->name, (const xmlChar *) "SN") == 0) {
                        sn = (char*)xmlNodeGetContent(current_node);
                    } else if (xmlStrcmp(current_node->name, (const xmlChar *) "DeviceID") == 0) {
                        deviceId = (char*)xmlNodeGetContent(current_node);
                    } else if (xmlStrcmp(current_node->name, (const xmlChar *) "PTZCmd") == 0) {
                        ptzCmd = (char*)xmlNodeGetContent(current_node);
                    }
                    current_node = current_node->next;
                }

                if (cmdType == "Catalog") {
                    std::cout << "Received Catalog query." << std::endl;
                    std::string responseXml = buildCatalogResponse(sn);
                    osip_message_t *answer = nullptr;
                    eXosip_message_build_answer(context_, request, 200, &answer);
                    osip_message_set_content_type(answer, "Application/MANSCDP+xml");
                    osip_message_set_body(answer, responseXml.c_str(), responseXml.length());
                    eXosip_message_send_answer(context_, ev->tid, answer);
                    std::cout << "Sent Catalog response." << std::endl;
                } else if (cmdType == "DeviceControl") {
                    std::cout << "Received DeviceControl (PTZ) command." << std::endl;
                    handleDeviceControl(ev, cmdType, sn, deviceId, ptzCmd);
                }
            }
            xmlFreeDoc(doc);
        }
    }
}

void Gb28181Client::handleDeviceControl(eXosip_event_t* ev, const std::string& cmdType, const std::string& sn, const std::string& deviceId, const std::string& ptzCmd) {
    // Decode and execute the PTZ command
    decodeAndExecutePtzCmd(ptzCmd);

    // Send 200 OK response
    osip_message_t *answer = nullptr;
    eXosip_message_build_answer(context_, ev->request, 200, &answer);
    eXosip_message_send_answer(context_, ev->tid, answer);
    std::cout << "Sent 200 OK for DeviceControl." << std::endl;
}

void Gb28181Client::decodeAndExecutePtzCmd(const std::string& ptzCmd) {
    if (ptzCmd.length() != 16) {
        std::cerr << "Invalid PTZCmd length: " << ptzCmd.length() << std::endl;
        return;
    }

    // Example decoding of the PTZ command string (simplified)
    // A1 0F 01 00 00 00 00 00
    // Byte 4: Direction
    // Byte 5: Horizontal Speed
    // Byte 6: Vertical Speed
    // Byte 7: Zoom Speed

    int byte4 = std::stoi(ptzCmd.substr(6, 2), nullptr, 16);
    int byte5 = std::stoi(ptzCmd.substr(8, 2), nullptr, 16);
    int byte6 = std::stoi(ptzCmd.substr(10, 2), nullptr, 16);
    int byte7 = std::stoi(ptzCmd.substr(12, 2), nullptr, 16);

    std::cout << "--- PTZ Command Decoded ---" << std::endl;
    if (byte4 == 0) {
        std::cout << "Action: STOP" << std::endl;
    } else {
        if (byte4 & 0x01) std::cout << "Action: TILT UP, Speed: " << byte6 << std::endl;
        if (byte4 & 0x02) std::cout << "Action: TILT DOWN, Speed: " << byte6 << std::endl;
        if (byte4 & 0x04) std::cout << "Action: PAN LEFT, Speed: " << byte5 << std::endl;
        if (byte4 & 0x08) std::cout << "Action: PAN RIGHT, Speed: " << byte5 << std::endl;
        if (byte4 & 0x10) std::cout << "Action: ZOOM IN, Speed: " << byte7 << std::endl;
        if (byte4 & 0x20) std::cout << "Action: ZOOM OUT, Speed: " << byte7 << std::endl;
    }
    std::cout << "---------------------------" << std::endl;

    // In a real application, you would send these commands to the actual camera hardware.
}

void Gb28181Client::handleMessageAnswer(eXosip_event_t* ev) {
    if (ev && ev->ack) {
        if (osip_message_get_status_code(ev->ack) == 200) {
            std::cout << "Received 200 OK for MESSAGE (e.g., Keep-alive)." << std::endl;
        }
    }
}

void Gb28181Client::handleInvite(eXosip_event_t* ev) {
    if (!ev || !ev->request) {
        return;
    }

    osip_message_t *request = ev->request;
    osip_body_t *body = nullptr;
    osip_message_get_body(request, 0, &body);

    std::string remoteIp;
    int remotePort = 0;
    int localRtpPort = 0;

    if (body && body->body) {
        std::cout << "Received SDP: " << body->body << std::endl;
        parseSdp(request, remoteIp, remotePort);
    }

    if (remotePort == 0) {
        std::cerr << "Failed to parse remote SDP for RealPlay." << std::endl;
        // Send error response
        osip_message_t *answer = nullptr;
        eXosip_message_build_answer(context_, request, 400, &answer); // Bad Request
        eXosip_message_send_answer(context_, ev->tid, answer);
        return;
    }

    localRtpPort = getAvailableRtpPort();
    if (localRtpPort == 0) {
        std::cerr << "Failed to get an available RTP port." << std::endl;
        osip_message_t *answer = nullptr;
        eXosip_message_build_answer(context_, request, 503, &answer); // Service Unavailable
        eXosip_message_send_answer(context_, ev->tid, answer);
        return;
    }

    // Build 200 OK with local SDP
    osip_message_t *answer = nullptr;
    eXosip_message_build_answer(context_, request, 200, &answer);
    osip_message_set_content_type(answer, "Application/sdp");

    std::string localSdp = buildSdpAnswer(remoteIp, remotePort, localRtpPort); 
    osip_message_set_body(answer, localSdp.c_str(), localSdp.length());
    eXosip_message_send_answer(context_, ev->tid, answer);
    std::cout << "Sent 200 OK for RealPlay INVITE. Local RTP Port: " << localRtpPort << std::endl;

    // Create and store RtpSession
    std::lock_guard<std::mutex> lock(rtpSessionsMutex_);
    rtpSessions_.emplace(ev->cid, RtpSession(remoteIp, remotePort, localRtpPort, ev->cid));
    RtpSession& currentSession = rtpSessions_.at(ev->cid);
    currentSession.rtpThread = std::thread(&Gb28181Client::startRtpStream, this, ev->cid, remoteIp, remotePort, localRtpPort, std::ref(currentSession.running));
}

void Gb28181Client::handleAck(eXosip_event_t* ev) {
    std::cout << "ACK received for call ID: " << ev->cid << ". RTP stream should be active." << std::endl;
}

void Gb28181Client::handleBye(eXosip_event_t* ev) {
    std::lock_guard<std::mutex> lock(rtpSessionsMutex_);
    auto it = rtpSessions_.find(ev->cid);
    if (it != rtpSessions_.end()) {
        it->second.stop(); // Stop the RTP thread and join it
        rtpSessions_.erase(it);
        std::cout << "RTP session for call ID " << ev->cid << " terminated." << std::endl;
    } else {
        std::cerr << "Error: BYE received for unknown call ID: " << ev->cid << std::endl;
    }
}

std::string Gb28181Client::buildCatalogResponse(const std::string& sn) {
    std::string response = "<?xml version=\"1.0\" encoding=\"GB2312\"?>\n";
    response += "<Response>\n";
    response += "  <CmdType>Catalog</CmdType>\n";
    response += "  <SN>" + sn + "</SN>\n";
    response += "  <DeviceID>" + deviceId_ + "</DeviceID>\n";
    response += "  <SumNum>1</SumNum>\n";
    response += "  <DeviceList Num=\'1\'>\n";
    response += "    <Item>\n";
    response += "      <DeviceID>" + deviceId_ + "01</DeviceID>\n"; // Example channel ID
    response += "      <Name>Camera 01</Name>\n";
    response += "      <Manufacturer>Manus</Manufacturer>\n";
    response += "      <Model>Model A</Model>\n";
    response += "      <Owner>Owner A</Owner>\n";
    response += "      <CivilCode>440300</CivilCode>\n";
    response += "      <Block>Block A</Block>\n";
    response += "      <Address>Address A</Address>\n";
    response += "      <Parental>1</Parental>\n";
    response += "      <ParentID>" + deviceId_ + "</ParentID>\n";
    response += "      <RegisterWay>1</RegisterWay>\n";
    response += "      <Secrecy>0</Secrecy>\n";
    response += "      <Status>ON</Status>\n";
    response += "      <Longitude>113.94</Longitude>\n";
    response += "      <Latitude>22.55</Latitude>\n";
    response += "      <StreamStatus>ON</StreamStatus>\n";
    response += "    </Item>\n";
    response += "  </DeviceList>\n";
    response += "</Response>";
    return response;
}

std::string Gb28181Client::buildKeepAliveMessage() {
    int current_sn = ++sn_counter;
    std::string xml = "<?xml version=\"1.0\"?>\n";
    xml += "<Notify>\n";
    xml += "  <CmdType>Keepalive</CmdType>\n";
    xml += "  <SN>" + std::to_string(current_sn) + "</SN>\n";
    xml += "  <DeviceID>" + deviceId_ + "</DeviceID>\n";
    xml += "  <Status>OK</Status>\n";
    xml += "</Notify>";
    return xml;
}

void Gb28181Client::parseSdp(osip_message_t* sdpMessage, std::string& remoteIp, int& remotePort) {
    osip_sdp_message_t *sdp = nullptr;
    osip_message_get_sdp(sdpMessage, &sdp);

    if (sdp) {
        if (sdp->c_list.nb_elt > 0) {
            osip_sdp_connection_t *c = (osip_sdp_connection_t*)osip_list_get_get(sdp->c_list, 0);
            if (c && c->connection_address) {
                remoteIp = c->connection_address;
            }
        }

        if (sdp->m_list.nb_elt > 0) {
            osip_sdp_media_t *m = (osip_sdp_media_t*)osip_list_get_get(sdp->m_list, 0);
            if (m && m->port) {
                remotePort = std::stoi(m->port);
            }
        }
        osip_sdp_message_free(sdp);
    }
    std::cout << "Parsed SDP: Remote IP = " << remoteIp << ", Remote Port = " << remotePort << std::endl;
}

std::string Gb28181Client::buildSdpAnswer(const std::string& remoteIp, int remotePort, int localRtpPort) {
    std::string sdp = "v=0\r\n";
    sdp += "o-" + deviceId_ + " 0 0 IN IP4 " + remoteIp + "\r\n"; 
    sdp += "s=Play\r\n";
    sdp += "c=IN IP4 " + remoteIp + "\r\n"; 
    sdp += "t=0 0\r\n";
    sdp += "m=video " + std::to_string(localRtpPort) + " RTP/AVP 96\r\n"; // Use dynamic localRtpPort
    sdp += "a=recvonly\r\n";
    sdp += "a=rtpmap:96 PS/90000\r\n"; 
    return sdp;
}

void Gb28181Client::startRtpStream(int callId, const std::string& remoteIp, int remotePort, int localRtpPort, std::atomic<bool>& runningFlag) {
    std::cout << "RTP Stream (Call ID: " << callId << ") starting to " << remoteIp << ":" << remotePort 
              << " from local port " << localRtpPort << std::endl;
    
    // This is a placeholder for actual RTP streaming logic.
    // In a real implementation, you would open a UDP socket on localRtpPort,
    // read video data, encapsulate it into RTP/PS packets, and send to remoteIp:remotePort.
    // For demonstration, we simulate pushing a dummy stream using FFmpeg to ZLMediaKit.
    
    // Construct FFmpeg command to push a test stream to ZLMediaKit
    // Note: ZLMediaKit should be running and configured to accept RTMP streams.
    // The stream_id should be unique for each concurrent stream.
    std::string streamId = deviceId_ + "_channel" + std::to_string(callId);
    std::string rtmpUrl = zlMediaKitPushUrl_ + streamId;

    // Example FFmpeg command: generate a test source and push as RTMP
    // This command will run in a separate process and push a dummy video stream.
    std::string ffmpegCmd = "ffmpeg -re -f lavfi -i testsrc=size=640x480:rate=30 -f lavfi -i sine=frequency=1000 -c:v libx264 -preset veryfast -tune zerolatency -c:a aac -ar 44100 -f flv " + rtmpUrl + " > /dev/null 2>&1";
    
    std::cout << "Executing FFmpeg command: " << ffmpegCmd << std::endl;

    // Use a pipe to manage the FFmpeg process
    FILE* pipe = popen(ffmpegCmd.c_str(), "w");
    if (!pipe) {
        std::cerr << "Failed to open pipe for FFmpeg command." << std::endl;
        return;
    }

    while (runningFlag) {
        // Keep the thread alive while FFmpeg is pushing. 
        // In a real scenario, this loop would manage reading from camera and sending RTP.
        std::this_thread::sleep_for(std::chrono::seconds(1)); 
    }

    // When streaming stops, close the FFmpeg process
    pclose(pipe);
    std::cout << "RTP Stream (Call ID: " << callId << ") stopped. FFmpeg process terminated." << std::endl;
}

int Gb28181Client::getAvailableRtpPort() {
    int port = nextRtpPort_.fetch_add(2); 
    if (port > RTP_PORT_END) {
        nextRtpPort_ = RTP_PORT_START; 
        port = nextRtpPort_.fetch_add(2);
        if (port > RTP_PORT_END) return 0; 
    }
    return port;
}
