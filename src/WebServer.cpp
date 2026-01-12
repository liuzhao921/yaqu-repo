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
    ss << "<link href=\"https://vjs.zencdn.net/7.11.4/video-js.css\" rel=\"stylesheet\" />\n";
    ss << "<style>\n";
    ss << "  body { font-family: sans-serif; margin: 20px; background-color: #f0f2f5; }\n";
    ss << "  h1 { color: #333; text-align: center; margin-bottom: 30px; }\n";
    ss << "  .stream-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }\n";
    ss << "  .stream-card { background-color: #fff; border: 1px solid #ddd; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); padding: 20px; }\n";
    ss << "  .stream-card h3 { margin-top: 0; color: #0056b3; }\n";
    ss << "  .video-js { width: 100%; height: 200px; }\n";
    ss << "  .stream-info { margin-top: 15px; font-size: 0.9em; color: #555; }\n";
    ss << "  .stream-info p { margin: 5px 0; }\n";
    ss << "</style>\n";
    ss << "</head>\n";
    ss << "<body>\n";
    ss << "<h1>Active GB28181 Streams</h1>\n";
    ss << "<div class=\"stream-grid\">\n";

    std::lock_guard<std::mutex> lock(streamsMutex_);
    if (activeStreams_.empty()) {
        ss << "<p>No active streams currently.</p>\n";
    } else {
        for (const auto& pair : activeStreams_) {
            const StreamInfo& info = pair.second;
            ss << "<div class=\"stream-card\">\n";
            ss << "  <h3>Device ID: " << info.deviceId << " (Stream ID: " << info.streamId << ")</h3>\n";
            ss << "  <video id=\"video-" << info.streamId << "\" class=\"video-js vjs-default-skin\" controls preload=\"auto\" width=\"640\" height=\"264\" data-setup=\"{}\">\n";
            ss << "    <source src=\"http://localhost:8080/live/" << info.streamId << ".m3u8\" type=\"application/x-mpegURL\">\n";
            ss << "    <p class=\"vjs-no-js\">\n";
            ss << "      To view this video please enable JavaScript, and consider upgrading to a web browser that\n";
            ss << "      <a href=\"https://videojs.com/html5-video-support/\" target=\"_blank\">supports HTML5 video</a>\n";
            ss << "    </p>\n";
            ss << "  </video>\n";
            ss << "  <div class=\"stream-info\">\n";
            ss << "    <p><strong>ZLMediaKit Push URL:</strong> " << info.rtmpUrl << "</p>\n";
            ss << "    <p><strong>HLS Playback:</strong> <a href=\"http://localhost:8080/live/" << info.streamId << ".m3u8\" target=\"_blank\">http://localhost:8080/live/" << info.streamId << ".m3u8</a></p>\n";
            ss << "    <p><strong>FLV Playback:</strong> <a href=\"http://localhost:8080/live/" << info.streamId << ".flv\" target=\"_blank\">http://localhost:8080/live/" << info.streamId << ".flv</a></p>\n";
            ss << "    <p><strong>WebRTC Playback:</strong> <a href=\"http://localhost:8080/webrtc/" << info.streamId << "\" target=\"_blank\">http://localhost:8080/webrtc/" << info.streamId << "</a></p>\n";
            ss << "  </div>\n";
            ss << "</div>\n";
        }
    }

    ss << "</div>\n";
    ss << "<script src=\"https://vjs.zencdn.net/7.11.4/video.min.js\"></script>\n";
    ss << "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/videojs-contrib-hls/5.15.0/videojs-contrib-hls.min.js\"></script>\n";
    ss << "<script>\n";
    ss << "  document.addEventListener('DOMContentLoaded', function() {\n";
    ss << "    var players = document.querySelectorAll('.video-js');\n";
    ss << "    players.forEach(function(playerElement) {\n";
    ss << "      var player = videojs(playerElement.id);\n";
    ss << "      player.play();\n";
    ss << "    });\n";
    ss << "  });\n";
    ss << "</script>\n";
    ss << "</body>\n";
    ss << "</html>\n";
    return ss.str();
}
