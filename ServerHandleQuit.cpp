#include "Server.hpp"

void Server::handleQuit(int clientFd, const ParsedCommand &command) {
    std::string reason = "Client Quit";
    if (!command.params.empty())
        reason = command.params[0];
    removeClient(clientFd, reason);
}
