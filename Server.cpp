#include "Server.hpp"

volatile sig_atomic_t Server::Signal = 0;

void Server::signalHandler(int signum) {
    (void)signum; // Suppress unused parameter warning
    Signal = 1;
}

void Server::initServer(int port, const std::string &password) {
    this->port = port;
    this->password = password;

    // 1. Setup the main listening socket
    setupServerSocket();

    std::cout << "Server started. Listening on port " << port << "...\n";

    // 2. Enter the main event loop
    runEventLoop();
}

void Server::setupServerSocket() {
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(this->port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    this->serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->serverFd == -1)
        throw std::runtime_error("Failed to create socket");

    int opt = 1;
    if (setsockopt(this->serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        throw std::runtime_error("Failed to set SO_REUSEADDR");

    if (fcntl(this->serverFd, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error("Failed to set O_NONBLOCK");

    if (bind(this->serverFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
        throw std::runtime_error("Failed to bind socket");

    if (listen(this->serverFd, SOMAXCONN) == -1)
        throw std::runtime_error("listen() failed");

    // Register the server socket for polling
    struct pollfd pfd;
    pfd.fd = this->serverFd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    this->fds.push_back(pfd);
}

void Server::runEventLoop() {
    while (Server::Signal == 0) {
        // Wait for an event on any registered socket
        if (poll(&fds[0], fds.size(), -1) == -1 && Server::Signal == 0) {
            throw std::runtime_error("poll() failed");
        }

        // Iterate through all file descriptors to check for events
        for (size_t i = 0; i < fds.size(); i++) {
            
            // Check if there is data to read
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == this->serverFd) {
                    acceptNewClient();
                } else {
                    handleClientData(fds[i].fd);
                }
            }
        }
    }
    // Loop breaks when Signal is caught (Ctrl+C)
    closeAllConnections();
}

void Server::acceptNewClient() {
    struct sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);

    int clientFd = accept(this->serverFd, (sockaddr *)&clientAddr, &len);
    if (clientFd == -1) {
        std::cerr << "accept() failed\n";
        return;
    }

    if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1) {
        std::cerr << "fcntl() failed for client\n";
        close(clientFd);
        return;
    }

    // Allocate Client object ONLY after connection is fully successful
    Client *newClient = new Client();
    newClient->setFd(clientFd);
    newClient->setIpAdd(inet_ntoa(clientAddr.sin_addr));
    
    this->clients[clientFd] = newClient;

    // Register the new client for polling
    struct pollfd pfd;
    pfd.fd = clientFd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    this->fds.push_back(pfd);

    std::cout << "Client <" << clientFd << "> connected.\n";
}

void Server::processBuffer(int clientFd) {
    std::string &buffer = this->clientBuffers[clientFd];
    size_t pos;

    // Process complete commands (terminated by \r\n)
    while ((pos = buffer.find("\r\n")) != std::string::npos) {
        std::string command = buffer.substr(0, pos);
        buffer.erase(0, pos + 2); // Remove processed command from buffer

        // Here you would parse and handle the command
        std::cout << "Received command from client <" << clientFd << ">: " << command << "\n";
        
        // For now, just echo back the command to the client
        std::string response = "You sent: " + command + "\r\n";
        send(clientFd, response.c_str(), response.length(), 0);
    }
}

void Server::handleClientData(int clientFd) {
    char buffer[1024];
    std::memset(buffer, 0, sizeof(buffer));

    ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead <= 0) {
        // Client disconnected or error
        std::cout << "Client <" << clientFd << "> disconnected.\n";
        removeClient(clientFd);
    } else {
        // Successfully received data
        buffer[bytesRead] = '\0';
        this->clientBuffers[clientFd].append(buffer);
        
        // Process the buffer (check for \r\n and parse commands)
        processBuffer(clientFd); 
    }
}

void Server::removeClient(int clientFd) {
    // 1. Close the socket
    close(clientFd);

    // 2. Remove from poll structure
    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i].fd == clientFd) {
            fds.erase(fds.begin() + i);
            break;
        }
    }

    // 3. Delete the Client object and remove from maps
    std::map<int, Client *>::iterator it = clients.find(clientFd);
    if (it != clients.end()) {
        delete it->second;
        clients.erase(it);
    }
    clientBuffers.erase(clientFd);
}

void Server::closeAllConnections() {
    std::cout << "\nShutting down server...\n";

    for (std::map<int, Client *>::iterator it = clients.begin(); it != clients.end(); ++it) {
        std::string quitMsg = "ERROR :Server shutting down\r\n";
        send(it->first, quitMsg.c_str(), quitMsg.length(), 0);
        close(it->first);
        delete it->second;
    }
    
    clients.clear();
    clientBuffers.clear();
    fds.clear();

    if (this->serverFd != -1) {
        close(this->serverFd);
        this->serverFd = -1;
    }
}

Server::Server() : port(0), password(""), serverFd(-1) {
}
Server::~Server() {
    closeAllConnections();
}