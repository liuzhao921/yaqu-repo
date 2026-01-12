#include "Gb28181Client.h"
#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <osip2/osip_sdp.h>

Gb28181Client::Gb28181Client(const std::string& serverIp, int serverPort, const std::string& deviceId, const std::string& realm, const std::string& password)
    : serverIp_(serverIp), serverPort_(serverPort), deviceId_(deviceId), realm_(realm), password_(password), running_(false), context_(nullptr), registerId_(-1) {
    
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
        // Start listening on a local port (e.g., 5060)
        if (eXosip_listen_addr(context_, IPPROTO_UDP, nullptr, 5060, AF_INET, 0) != 0) {
            std::cerr << "Failed to listen on port 5060" << std::endl;
            return;
        }

        running_ = true;

        // Build initial REGISTER message
        std::string from = "sip:" + deviceId_ + "@" + realm_;
        std::string proxy = "sip:" + serverIp_ + ":" + std::to_string(serverPort_);
        osip_message_t *reg = nullptr;

        registerId_ = eXosip_register_build_initial_register(context_, from.c_str(), proxy.c_str(), nullptr, 3600, &reg);
        if (registerId_ > 0) {
            eXosip_add_authentication_info(context_, deviceId_.c_str(), deviceId_.c_str(), password_.c_str(), nullptr, realm_.c_str());
            eXosip_register_send_register(context_, registerId_, reg);
        }

        eventThread_ = std::thread(&Gb28181Client::eventLoop, this);
        std::cout << "GB28181 Client started with eXosip2." << std::endl;
    }
}

void Gb28181Client::stop() {
    if (running_) {
        running_ = false;
        if (eventThread_.joinable()) {
            eventThread_.join();
        }
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
            case EXOSIP_CALL_INVITE:
                std::cout << "GB28181: New INVITE received (RealPlay request)" << std::endl;
                handleInvite(ev);
                break;
            default:
                std::cout << "GB28181: Received event type: " << ev->type << std::endl;
                break;
        }
        eXosip_event_free(ev);
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
            if (root_element && xmlStrcmp(root_element->name, (const xmlChar *) "Query") == 0) {
                xmlNodePtr cmdType_node = root_element->children;
                while (cmdType_node) {
                    if (xmlStrcmp(cmdType_node->name, (const xmlChar *) "CmdType") == 0) {
                        std::string cmdType = (char*)xmlNodeGetContent(cmdType_node);
                        if (cmdType == "Catalog") {
                            std::cout << "Received Catalog query." << std::endl;
                            // Extract SN for response
                            std::string sn;
                            xmlNodePtr sn_node = root_element->children;
                            while(sn_node) {
                                if (xmlStrcmp(sn_node->name, (const xmlChar *) "SN") == 0) {
                                    sn = (char*)xmlNodeGetContent(sn_node);
                                    break;
                                }
                                sn_node = sn_node->next;
                            }

                            // Build and send response
                            std::string responseXml = buildCatalogResponse(sn);
                            osip_message_t *answer = nullptr;
                            eXosip_message_build_answer(context_, request, 200, &answer);
                            osip_message_set_content_type(answer, "Application/MANSCDP+xml");
                            osip_message_set_body(answer, responseXml.c_str(), responseXml.length());
                            eXosip_message_send_answer(context_, ev->tid, answer);
                            std::cout << "Sent Catalog response." << std::endl;
                        }
                        break;
                    }
                    cmdType_node = cmdType_node->next;
                }
            }
            xmlFreeDoc(doc);
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

    // Build 200 OK with local SDP
    osip_message_t *answer = nullptr;
    eXosip_message_build_answer(context_, request, 200, &answer);
    osip_message_set_content_type(answer, "Application/sdp");

    std::string localSdp = buildSdpAnswer(remoteIp, remotePort); // Use remoteIp for local SDP 'c=' line
    osip_message_set_body(answer, localSdp.c_str(), localSdp.length());
    eXosip_message_send_answer(context_, ev->tid, answer);
    std::cout << "Sent 200 OK for RealPlay INVITE." << std::endl;

    // Wait for ACK (simplified, in a real scenario, this would be part of the event loop)
    // For now, we assume ACK will arrive and proceed to stream.
    std::cout << "Waiting for ACK... (simulated)" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1)); // Simulate waiting for ACK

    // Start RTP streaming (placeholder)
    startRtpStream(remoteIp, remotePort);
}

std::string Gb28181Client::buildCatalogResponse(const std::string& sn) {
    // This is a simplified example. In a real scenario, you would query actual device channels.
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

void Gb28181Client::parseSdp(osip_message_t* sdpMessage, std::string& remoteIp, int& remotePort) {
    // This is a simplified SDP parser. In a real scenario, you would use a dedicated SDP parsing library
    // or more robust osip functions.
    osip_sdp_message_t *sdp = nullptr;
    osip_message_get_sdp(sdpMessage, &sdp);

    if (sdp) {
        // Extract remote IP from 'c=' line
        if (sdp->c_list.nb_elt > 0) {
            osip_sdp_connection_t *c = (osip_sdp_connection_t*)osip_list_get_get(sdp->c_list, 0);
            if (c && c->connection_address) {
                remoteIp = c->connection_address;
            }
        }

        // Extract remote port from 'm=' line
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

std::string Gb28181Client::buildSdpAnswer(const std::string& remoteIp, int remotePort) {
    // This builds a simplified SDP answer. A real implementation would be more complex.
    std::string sdp = "v=0\r\n";
    sdp += "o-" + deviceId_ + " 0 0 IN IP4 " + remoteIp + "\r\n"; // Use remoteIp for local SDP 'o=' line
    sdp += "s=Play\r\n";
    sdp += "c=IN IP4 " + remoteIp + "\r\n"; // Use remoteIp for local SDP 'c=' line
    sdp += "t=0 0\r\n";
    sdp += "m=video 5000 RTP/AVP 96\r\n"; // Example: video on port 5000, payload type 96
    sdp += "a=recvonly\r\n";
    sdp += "a=rtpmap:96 PS/90000\r\n"; // PS stream, 90000 clock rate
    return sdp;
}

void Gb28181Client::startRtpStream(const std::string& remoteIp, int remotePort) {
    std::cout << "Starting RTP stream to " << remoteIp << ":" << remotePort << " (placeholder)" << std::endl;
    // In a real implementation, this would initiate the actual RTP/RTCP streaming process.
    // This would involve: 
    // 1. Opening a UDP socket.
    // 2. Reading video frames (e.g., from a camera or file).
    // 3. Encapsulating frames into PS (Program Stream) packets (if not already).
    // 4. Encapsulating PS packets into RTP packets.
    // 5. Sending RTP packets to remoteIp:remotePort.
}
