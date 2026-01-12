#include "Gb28181Client.h"
#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <osip2/osip_sdp.h>
#include <random>

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

void Gb28181Client::handleInvite(eXosip_event_t* ev) { /* ... existing code ... */ }

void Gb28181Client::handleAck(eXosip_event_t* ev) { /* ... existing code ... */ }

void Gb28181Client::handleBye(eXosip_event_t* ev) { /* ... existing code ... */ }

std::string Gb28181Client::buildCatalogResponse(const std::string& sn) { /* ... existing code ... */ }

std::string Gb28181Client::buildKeepAliveMessage() { /* ... existing code ... */ }

void Gb28181Client::parseSdp(osip_message_t* sdpMessage, std::string& remoteIp, int& remotePort) { /* ... existing code ... */ }

std::string Gb28181Client::buildSdpAnswer(const std::string& remoteIp, int remotePort, int localRtpPort) { /* ... existing code ... */ }

void Gb28181Client::startRtpStream(int callId, const std::string& remoteIp, int remotePort, int localRtpPort, std::atomic<bool>& runningFlag) { /* ... existing code ... */ }

int Gb28181Client::getAvailableRtpPort() { /* ... existing code ... */ }
