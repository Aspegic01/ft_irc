#include "Server.hpp"
#include <iostream>
#include <cstdlib>
int main(int ac, char **av)
{
    int port;
    std::string password;
    Server server;
    if (ac != 3){
        std::cerr << "Usage: ./ircserv <port> <password>" << std::endl;
        return 1;
    }
    
    port = std::atoi(av[1]);
    if (port < 1 || port > 65535) {
        std::cerr << "Error: Invalid port number. Please provide a port number between 1 and 65535." << std::endl;
        return 1;
    }
    password = av[2];
    if (password.empty()) {
        std::cerr << "Error: Password cannot be empty." << std::endl;
        return 1;
    }
    try{
        signal(SIGINT, Server::signalHandler);
        signal(SIGQUIT, Server::signalHandler);
        server.initServer(port, password);
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "The Server is closed." << std::endl;
}