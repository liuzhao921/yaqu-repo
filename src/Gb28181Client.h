#ifndef GB28181_CLIENT_H
#define GB28181_CLIENT_H

#include <string>
#include <thread>
#include <iostream>
#include <chrono>

class Gb28181Client {
public:
    Gb28181Client(const std::string& serverIp, int serverPort, const std::string& deviceId, const std::string& realm);
    ~Gb28181Client();

    void start();
    void stop();

private:
    void registerLoop();
    void keepAliveLoop();

    std::string serverIp_;
    int serverPort_;
    std::string deviceId_;
    std::string realm_;
    bool running_;
    std::thread registerThread_;
    std::thread keepAliveThread_;
};

#endif // GB28181_CLIENT_H
