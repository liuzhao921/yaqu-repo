#ifndef GB28181_CLIENT_H
#define GB28181_CLIENT_H

#include <string>
#include <thread>
#include <iostream>
#include <chrono>
#include <eXosip2/eXosip2.h>

class Gb28181Client {
public:
    Gb28181Client(const std::string& serverIp, int serverPort, const std::string& deviceId, const std::string& realm, const std::string& password);
    ~Gb28181Client();

    void start();
    void stop();

private:
    void eventLoop();

    std::string serverIp_;
    int serverPort_;
    std::string deviceId_;
    std::string realm_;
    std::string password_;
    
    bool running_;
    eXosip_t* context_;
    int registerId_;
    std::thread eventThread_;
};

#endif // GB28181_CLIENT_H
