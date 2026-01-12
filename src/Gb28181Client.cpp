#include "Gb28181Client.h"

Gb28181Client::Gb28181Client(const std::string& serverIp, int serverPort, const std::string& deviceId, const std::string& realm)
    : serverIp_(serverIp), serverPort_(serverPort), deviceId_(deviceId), realm_(realm), running_(false) {
    std::cout << "GB28181 Client initialized for device: " << deviceId_ << std::endl;
}

Gb28181Client::~Gb28181Client() {
    stop();
}

void Gb28181Client::start() {
    if (!running_) {
        running_ = true;
        registerThread_ = std::thread(&Gb28181Client::registerLoop, this);
        keepAliveThread_ = std::thread(&Gb28181Client::keepAliveLoop, this);
        std::cout << "GB28181 Client started." << std::endl;
    }
}

void Gb28181Client::stop() {
    if (running_) {
        running_ = false;
        if (registerThread_.joinable()) {
            registerThread_.join();
        }
        if (keepAliveThread_.joinable()) {
            keepAliveThread_.join();
        }
        std::cout << "GB28181 Client stopped." << std::endl;
    }
}

void Gb28181Client::registerLoop() {
    while (running_) {
        std::cout << "GB28181 Client: Sending registration for device " << deviceId_ << " to " << serverIp_ << ":" << serverPort_ << std::endl;
        // Simulate SIP registration process
        std::this_thread::sleep_for(std::chrono::seconds(30)); // Register every 30 seconds
    }
}

void Gb28181Client::keepAliveLoop() {
    while (running_) {
        std::cout << "GB28181 Client: Sending keep-alive for device " << deviceId_ << std::endl;
        // Simulate SIP keep-alive process
        std::this_thread::sleep_for(std::chrono::seconds(60)); // Keep-alive every 60 seconds
    }
}
