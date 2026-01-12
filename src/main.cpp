#include <iostream>
#include "Gb28181Client.h"
#include "WebServer.h"

int main() {
    std::cout << "Device Access Module Starting..." << std::endl;

    // Initialize GB28181 Client
    Gb28181Client gbClient("192.168.1.100", 5060, "34020000001320000001", "3402000000");
    gbClient.start();

    // Initialize Web Server for video streaming
    WebServer webServer(8080);
    webServer.start();

    std::cout << "Device Access Module Running." << std::endl;

    // Keep the main thread alive
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
