#include "WebServer.h"
#include <sstream>

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

void WebServer::addStream(const StreamInfo& info) {
    std::lock_guard<std::mutex> lock(streamsMutex_);
    activeStreams_[info.streamId] = info;
    std::cout << "Added stream: " << info.streamId << " to WebServer." << std::endl;
}

void WebServer::removeStream(const std::string& streamId) {
    std::lock_guard<std::mutex> lock(streamsMutex_);
    if (activeStreams_.count(streamId)) {
        activeStreams_.erase(streamId);
        std::cout << "Removed stream: " << streamId << " from WebServer." << std::endl;
    }
}

void WebServer::serverLoop() {
    // This is a simulated web server loop.
    // In a real application, you would use a library like Crow, Civetweb, or Boost.Beast
    // to handle HTTP requests and serve content.
    std::cout << "Web Server: Listening for connections on port " << port_ << std::endl;
    while (running_) {
        // Simulate handling a request every few seconds
        std::this_thread::sleep_for(std::chrono::seconds(5)); 
        // For demonstration, we'll just print the current index page content
        // In a real scenario, this would be served via HTTP.
        // std::cout << "--- Current Web Index Page ---\n" << buildIndexPage() << "\n------------------------------" << std::endl;
    }
}

std::string WebServer::buildIndexPage() {
    std::stringstream ss;
    ss << "<!DOCTYPE html>\n";
    ss << "<html>\n";
    ss << "<head>\n";
    ss << "<title>GB28181 Stream Viewer</title>\n";
    ss << "<style>\n";
    ss << "  body { font-family: sans-serif; margin: 20px; }\n";
    ss << "  .stream-card { border: 1px solid #ccc; padding: 15px; margin-bottom: 10px; border-radius: 5px; }\n";
    ss << "  .stream-card h3 { margin-top: 0; }\n";
    ss << "  .stream-card a { display: block; margin-bottom: 5px; }\n";
    ss << "</style>\n";
    ss << "</head>\n";
    ss << "<body>\n";
    ss << "<h1>Active GB28181 Streams</h1>\n";

    std::lock_guard<std::mutex> lock(streamsMutex_);
    if (activeStreams_.empty()) {
        ss << "<p>No active streams currently.</p>\n";
    } else {
        for (const auto& pair : activeStreams_) {
            const StreamInfo& info = pair.second;
            ss << "<div class=\"stream-card\">\n";
            ss << "  <h3>Device ID: " << info.deviceId << " (Stream ID: " << info.streamId << ")</h3>\n";
            ss << "  <p>ZLMediaKit Push URL: " << info.rtmpUrl << "</p>\n";
            ss << "  <p>Web Playback Links (assuming ZLMediaKit is running on port 8080):</p>\n";
            ss << "  <a href=\"http://localhost:8080/live/" << info.streamId << ".flv\" target=\"_blank\">HTTP-FLV</a>\n";
            ss << "  <a href=\"http://localhost:8080/live/" << info.streamId << ".m3u8\" target=\"_blank\">HLS</a>\n";
            ss << "  <a href=\"http://localhost:8080/webrtc/" << info.streamId << "\" target=\"_blank\">WebRTC</a>\n";
            ss << "</div>\n";
        }
    }

    ss << "</body>\n";
    ss << "</html>\n";
    return ss.str();
}
