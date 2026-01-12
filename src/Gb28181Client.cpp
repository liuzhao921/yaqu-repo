#include "Gb28181Client.h"
#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>

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

std::string Gb28181Client::buildCatalogResponse(const std::string& sn) {
    // This is a simplified example. In a real scenario, you would query actual device channels.
    std::string response = "<?xml version=\"1.0\" encoding=\"GB2312\"?>\n";
    response += "<Response>\n";
    response += "  <CmdType>Catalog</CmdType>\n";
    response += "  <SN>" + sn + "</SN>\n";
    response += "  <DeviceID>" + deviceId_ + "</DeviceID>\n";
    response += "  <SumNum>1</SumNum>\n";
    response += "  <DeviceList Num='1'>\n";
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
