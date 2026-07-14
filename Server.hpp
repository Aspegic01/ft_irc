#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cstring>
#include <csignal>

// Networking headers
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

// Forward declaration of your Client class
#include "Client.hpp" 

class Server {
private:
    int port;
    std::string password;
    int serverFd;
    
    // Core data structures for tracking connections
    std::vector<struct pollfd> fds;
    std::map<int, Client*> clients;
    std::map<int, std::string> clientBuffers;

    // -------------------------------------------------------------
    // Private Methods (Internal Server Logic)
    // -------------------------------------------------------------
    void setupServerSocket();
    void runEventLoop();
    void acceptNewClient();
    void handleClientData(int clientFd);
    void processBuffer(int clientFd);
    void removeClient(int clientFd);
    void closeAllConnections();

    // -------------------------------------------------------------
    // Orthodox Canonical Form Protection
    // -------------------------------------------------------------
    // Made private to prevent accidental copying of file descriptors
    Server(const Server &other);
    Server &operator=(const Server &other);

public:
    // Constructor & Destructor
    Server();
    ~Server();

    // Signal handling
    static volatile sig_atomic_t Signal;
    static void signalHandler(int signum);

    // Initialization
    void initServer(int port, const std::string &password);
};

#endif // SERVER_HPP