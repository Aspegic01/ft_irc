#include "Server.hpp"

void Server::handleUser(int clientFd, const ParsedCommand &command) {
    Client *client = clients[clientFd];

    if (client->getIsRegistered()) {
        sendNumeric(clientFd, 462, ":You may not reregister");
        return;
    }
    if (command.params.size() < 4) {
        sendNumeric(clientFd, 461, "USER :Not enough parameters");
        return;
    }

    client->setUsername(command.params[0]);
    client->setRealname(command.params[3]);
    client->setUserSet(true);

    maybeRegisterClient(clientFd);
}
