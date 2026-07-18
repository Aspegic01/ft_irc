#include "Server.hpp"

void Server::handleKick(int clientFd, const ParsedCommand &command) {
    if (command.params.size() < 2) {
        sendNumeric(clientFd, 461, "KICK :Not enough parameters");
        return;
    }

    std::string channelName = command.params[0];
    std::string nick = command.params[1];
    std::string reason = command.params.size() > 2 ? command.params[2] : clients[clientFd]->getNickname();

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

    std::map<std::string, int>::iterator nickIt = nickToFd.find(nick);
    if (nickIt == nickToFd.end() || channel.members.find(nickIt->second) == channel.members.end()) {
        sendNumeric(clientFd, 441, nick + " " + channelName + " :They aren't on that channel");
        return;
    }

    std::string kickMsg = getClientPrefix(clientFd) + " KICK " + channelName + " " + nick + " :" + reason + "\r\n";
    broadcastToChannel(channelName, kickMsg, -1);
    removeClientFromChannel(nickIt->second, channelName, "", false);
}
