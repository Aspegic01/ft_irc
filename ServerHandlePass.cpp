#include "Server.hpp"

void Server::handlePass(int clientFd, const ParsedCommand &command) {
    Client *client = clients[clientFd];

    if (client->getIsRegistered()) {
        sendNumeric(clientFd, 462, ":You may not reregister");
        return;
    }
    if (command.params.empty()) {
        sendNumeric(clientFd, 461, "PASS :Not enough parameters");
        return;
    }
    if (command.params[0] != password) {
        sendNumeric(clientFd, 464, ":Password incorrect");
        return;
    }

    client->setPassAccepted(true);
    maybeRegisterClient(clientFd);
}
