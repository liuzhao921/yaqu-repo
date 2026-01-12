#include "Gb28181Client.h"
#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <osip2/osip_sdp.h>

// For generating a unique SN (sequence number)
#include <atomic>
std::atomic<int> sn_counter(0);

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

void Gb28181Client::handleMessage(eXosip_event_t* ev) { /* ... existing code ... */ }

void Gb28181Client::handleMessageAnswer(eXosip_event_t* ev) {
    if (ev && ev->ack) {
        if (osip_message_get_status_code(ev->ack) == 200) {
            std::cout << "Received 200 OK for MESSAGE (e.g., Keep-alive)." << std::endl;
        }
    }
}

void Gb28181Client::handleInvite(eXosip_event_t* ev) { /* ... existing code ... */ }

std::string Gb28181Client::buildCatalogResponse(const std::string& sn) { /* ... existing code ... */ }

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

void Gb28181Client::parseSdp(osip_message_t* sdpMessage, std::string& remoteIp, int& remotePort) { /* ... existing code ... */ }

std::string Gb28181Client::buildSdpAnswer(const std::string& remoteIp, int remotePort) { /* ... existing code ... */ }

void Gb28181Client::startRtpStream(const std::string& remoteIp, int remotePort) { /* ... existing code ... */ }
