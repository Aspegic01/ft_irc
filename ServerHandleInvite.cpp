#include "Server.hpp"

void Server::handleInvite(int clientFd, const ParsedCommand &command) {
    if (command.params.size() < 2) {
        sendNumeric(clientFd, 461, "INVITE :Not enough parameters");
        return;
    }

    std::string nick = command.params[0];
    std::string channelName = command.params[1];

    std::map<std::string, int>::iterator nickIt = nickToFd.find(nick);
    if (nickIt == nickToFd.end()) {
        sendNumeric(clientFd, 401, nick + " :No such nick/channel");
        return;
    }

    std::map<std::string, Channel>::iterator chIt = channels.find(channelName);
    if (chIt == channels.end()) {
        sendNumeric(clientFd, 403, channelName + " :No such channel");
        return;
    }

    Channel &channel = chIt->second;
    if (channel.members.find(clientFd) == channel.members.end()) {
        sendNumeric(clientFd, 442, channelName + " :You're not on that channel");
        return;
    }
    if (channel.operators.find(clientFd) == channel.operators.end()) {
        sendNumeric(clientFd, 482, channelName + " :You're not channel operator");
        return;
    }
    if (channel.members.find(nickIt->second) != channel.members.end()) {
        sendNumeric(clientFd, 443, nick + " " + channelName + " :is already on channel");
        return;
    }

    channel.invited.insert(nickIt->second);
    sendNumeric(clientFd, 341, nick + " " + channelName);
    sendRaw(nickIt->second, getClientPrefix(clientFd) + " INVITE " + nick + " :" + channelName + "\r\n");
}
