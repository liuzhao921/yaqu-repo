#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <string>
#include <thread>
#include <iostream>
#include <chrono>

// For a real web server, you would integrate a library like Crow, Civetweb, or Boost.Beast
// This is a placeholder for demonstration purposes.

class WebServer {
public:
    WebServer(int port);
    ~WebServer();

    void start();
    void stop();

private:
    void serverLoop();

    int port_;
    bool running_;
    std::thread serverThread_;
};

#endif // WEB_SERVER_H
