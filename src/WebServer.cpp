#include "WebServer.h"

WebServer::WebServer(int port)
    : port_(port), running_(false) {
    std::cout << "Web Server initialized on port: " << port_ << std::endl;
}

WebServer::~WebServer() {
    stop();
}

void WebServer::start() {
    if (!running_) {
        running_ = true;
        serverThread_ = std::thread(&WebServer::serverLoop, this);
        std::cout << "Web Server started." << std::endl;
    }
}

void WebServer::stop() {
    if (running_) {
        running_ = false;
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        std::cout << "Web Server stopped." << std::endl;
    }
}

void WebServer::serverLoop() {
    while (running_) {
        // In a real application, this would handle incoming HTTP requests
        // and serve video streams (e.g., by converting RTSP to HLS/WebRTC).
        std::cout << "Web Server: Listening for connections on port " << port_ << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Simulate server activity
    }
}
