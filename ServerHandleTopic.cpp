#include "Server.hpp"

void Server::handleTopic(int clientFd, const ParsedCommand &command) {
    if (command.params.empty()) {
        sendNumeric(clientFd, 461, "TOPIC :Not enough parameters");
        return;
    }

    std::string channelName = command.params[0];
    std::map<std::string, Channel>::iterator it = channels.find(channelName);
    if (it == channels.end()) {
        sendNumeric(clientFd, 403, channelName + " :No such channel");
        return;
    }

    Channel &channel = it->second;
    if (channel.members.find(clientFd) == channel.members.end()) {
        sendNumeric(clientFd, 442, channelName + " :You're not on that channel");
        return;
    }

    if (command.params.size() == 1) {
        if (channel.topic.empty())
            sendNumeric(clientFd, 331, channelName + " :No topic is set");
        else
            sendNumeric(clientFd, 332, channelName + " :" + channel.topic);
        return;
    }

    if (channel.topicRestricted && channel.operators.find(clientFd) == channel.operators.end()) {
        sendNumeric(clientFd, 482, channelName + " :You're not channel operator");
        return;
    }

    channel.topic = command.params[1];
    std::string topicMsg = getClientPrefix(clientFd) + " TOPIC " + channelName + " :" + channel.topic + "\r\n";
    broadcastToChannel(channelName, topicMsg, -1);
}
