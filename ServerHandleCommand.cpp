#include "Server.hpp"

void Server::handleCommand(int clientFd, const ParsedCommand &command) {
    if (clients.find(clientFd) == clients.end())
        return;

    const std::string &cmd = command.command;

    if (cmd == "CAP")
        return;
    if (cmd == "PASS") {
        handlePass(clientFd, command);
        return;
    }
    if (cmd == "NICK") {
        handleNick(clientFd, command);
        return;
    }
    if (cmd == "USER") {
        handleUser(clientFd, command);
        return;
    }
    if (cmd == "PING") {
        handlePing(clientFd, command);
        return;
    }
    if (cmd == "PONG") {
        handlePong(clientFd, command);
        return;
    }
    if (cmd == "QUIT") {
        handleQuit(clientFd, command);
        return;
    }

    if (!clients[clientFd]->getIsRegistered()) {
        sendNumeric(clientFd, 451, ":You have not registered");
        return;
    }

    if (cmd == "JOIN")
        handleJoin(clientFd, command);
    else if (cmd == "PART")
        handlePart(clientFd, command);
    else if (cmd == "PRIVMSG")
        handlePrivmsg(clientFd, command, false);
    else if (cmd == "NOTICE")
        handlePrivmsg(clientFd, command, true);
    else if (cmd == "TOPIC")
        handleTopic(clientFd, command);
    else if (cmd == "INVITE")
        handleInvite(clientFd, command);
    else if (cmd == "KICK")
        handleKick(clientFd, command);
    else if (cmd == "MODE")
        handleMode(clientFd, command);
    else
        sendNumeric(clientFd, 421, cmd + " :Unknown command");
}
