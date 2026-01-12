#ifndef GB28181_CLIENT_H
#define GB28181_CLIENT_H

#include <string>
#include <thread>
#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <eXosip2/eXosip2.h>

// Forward declaration for osip_message_t
struct osip_message;
typedef struct osip_message osip_message_t;

// Structure to hold information for each RTP session
struct RtpSession {
    std::string remoteIp;
    int remotePort;
    int localRtpPort; // The local port this device will send RTP from
    std::thread rtpThread; // Thread for actual RTP streaming
    bool running; // Flag to control the RTP streaming thread
    int callId; // eXosip call ID for this session

    RtpSession(std::string ip, int r_port, int l_port, int c_id) 
        : remoteIp(std::move(ip)), remotePort(r_port), localRtpPort(l_port), running(true), callId(c_id) {}

    // Stop the RTP thread gracefully
    void stop() {
        running = false;
        if (rtpThread.joinable()) {
            rtpThread.join();
        }
    }
};

class Gb28181Client {
public:
    Gb28181Client(const std::string& serverIp, int serverPort, const std::string& deviceId, const std::string& realm, const std::string& password);
    ~Gb28181Client();

    void start();
    void stop();

private:
    void eventLoop();
    void keepAliveLoop();
    void handleMessage(eXosip_event_t* ev);
    void handleInvite(eXosip_event_t* ev);
    void handleAck(eXosip_event_t* ev);
    void handleBye(eXosip_event_t* ev);
    void handleMessageAnswer(eXosip_event_t* ev);

    std::string buildCatalogResponse(const std::string& sn);
    std::string buildSdpAnswer(const std::string& remoteIp, int remotePort, int localRtpPort);
    std::string buildKeepAliveMessage();
    void parseSdp(osip_message_t* sdpMessage, std::string& remoteIp, int& remotePort);
    void startRtpStream(int callId, const std::string& remoteIp, int remotePort, int localRtpPort, std::atomic<bool>& runningFlag);
    int getAvailableRtpPort();

    // New: PTZ control functions
    void handleDeviceControl(eXosip_event_t* ev, const std::string& cmdType, const std::string& sn, const std::string& deviceId, const std::string& ptzCmd);
    void decodeAndExecutePtzCmd(const std::string& ptzCmd);

    std::string serverIp_;
    int serverPort_;
    std::string deviceId_;
    std::string realm_;
    std::string password_;
    
    bool running_;
    eXosip_t* context_;
    int registerId_;
    std::thread eventThread_;
    std::thread keepAliveThread_;

    std::map<int, RtpSession> rtpSessions_; // Map callId to RtpSession
    std::mutex rtpSessionsMutex_; // Mutex for protecting rtpSessions_
    std::atomic<int> nextRtpPort_; // For dynamic RTP port allocation
};

#endif // GB28181_CLIENT_H
