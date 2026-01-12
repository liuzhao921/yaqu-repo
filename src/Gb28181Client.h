#ifndef GB28181_CLIENT_H
#define GB28181_CLIENT_H

#include <string>
#include <thread>
#include <iostream>
#include <chrono>
#include <vector>
#include <eXosip2/eXosip2.h>

// Forward declaration for osip_message_t
struct osip_message;
typedef struct osip_message osip_message_t;

class Gb28181Client {
public:
    Gb28181Client(const std::string& serverIp, int serverPort, const std::string& deviceId, const std::string& realm, const std::string& password);
    ~Gb28181Client();

    void start();
    void stop();

private:
    void eventLoop();
    void keepAliveLoop(); // New: for sending periodic keep-alive messages
    void handleMessage(eXosip_event_t* ev);
    void handleInvite(eXosip_event_t* ev);
    void handleMessageAnswer(eXosip_event_t* ev); // New: to handle responses to MESSAGE requests

    std::string buildCatalogResponse(const std::string& sn);
    std::string buildSdpAnswer(const std::string& remoteIp, int remotePort);
    std::string buildKeepAliveMessage(); // New: to construct keep-alive XML
    void parseSdp(osip_message_t* sdpMessage, std::string& remoteIp, int& remotePort);
    void startRtpStream(const std::string& remoteIp, int remotePort);

    std::string serverIp_;
    int serverPort_;
    std::string deviceId_;
    std::string realm_;
    std::string password_;
    
    bool running_;
    eXosip_t* context_;
    int registerId_;
    std::thread eventThread_;
    std::thread keepAliveThread_; // New: thread for keep-alive
};

#endif // GB28181_CLIENT_H
