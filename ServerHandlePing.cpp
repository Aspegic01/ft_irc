#include "Server.hpp"

void Server::handlePing(int clientFd, const ParsedCommand &command) {
    if (command.params.empty()) {
        sendNumeric(clientFd, 409, ":No origin specified");
        return;
    }
    sendRaw(clientFd, ":ircserv PONG ircserv :" + command.params[0] + "\r\n");
}
