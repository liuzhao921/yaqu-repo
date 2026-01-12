#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <string>
#include <thread>
#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <mutex>

// For a real web server, you would integrate a library like Crow, Civetweb, or Boost.Beast
// This is a placeholder for demonstration purposes.

struct StreamInfo {
    std::string deviceId;
    std::string streamId;
    std::string rtmpUrl;
    std::string flvUrl;
    std::string hlsUrl;
    std::string webrtcUrl;
};

class WebServer {
public:
    WebServer(int port);
    ~WebServer();

    void start();
    void stop();
    void addStream(const StreamInfo& info);
    void removeStream(const std::string& streamId);

private:
    void serverLoop();
    std::string buildIndexPage();

    int port_;
    bool running_;
    std::thread serverThread_;

    std::map<std::string, StreamInfo> activeStreams_; // Map streamId to StreamInfo
    std::mutex streamsMutex_; // Mutex for protecting activeStreams_
};

#endif // WEB_SERVER_H
