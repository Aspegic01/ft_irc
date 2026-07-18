#include "Server.hpp"

static std::vector<std::string> splitByComma(const std::string &value) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= value.size()) {
        size_t comma = value.find(',', start);
        if (comma == std::string::npos) {
            out.push_back(value.substr(start));
            break;
        }
        out.push_back(value.substr(start, comma - start));
        start = comma + 1;
    }
    return out;
}

void Server::handleJoin(int clientFd, const ParsedCommand &command) {
    if (command.params.empty()) {
        sendNumeric(clientFd, 461, "JOIN :Not enough parameters");
        return;
    }

    std::vector<std::string> channelNames = splitByComma(command.params[0]);
    std::vector<std::string> keys;
    if (command.params.size() > 1)
        keys = splitByComma(command.params[1]);

    for (size_t i = 0; i < channelNames.size(); ++i) {
        const std::string &channelName = channelNames[i];
        std::string key = (i < keys.size()) ? keys[i] : "";

        if (!isChannelNameValid(channelName)) {
            sendNumeric(clientFd, 403, channelName + " :No such channel");
            continue;
        }

        std::map<std::string, Channel>::iterator it = channels.find(channelName);
        if (it == channels.end()) {
            Channel channel;
            channel.name = channelName;
            channel.members.insert(clientFd);
            channel.operators.insert(clientFd);
            channels[channelName] = channel;
            it = channels.find(channelName);
        }

        Channel &channel = it->second;
        if (channel.members.find(clientFd) != channel.members.end())
            continue;

        if (channel.inviteOnly && channel.invited.find(clientFd) == channel.invited.end()) {
            sendNumeric(clientFd, 473, channelName + " :Cannot join channel (+i)");
            continue;
        }
        if (!channel.key.empty() && channel.key != key) {
            sendNumeric(clientFd, 475, channelName + " :Cannot join channel (+k)");
            continue;
        }
        if (channel.userLimit >= 0 && static_cast<int>(channel.members.size()) >= channel.userLimit) {
            sendNumeric(clientFd, 471, channelName + " :Cannot join channel (+l)");
            continue;
        }

        channel.members.insert(clientFd);
        channel.invited.erase(clientFd);

        std::string joinMsg = getClientPrefix(clientFd) + " JOIN " + channelName + "\r\n";
        broadcastToChannel(channelName, joinMsg, -1);

        if (!channel.topic.empty())
            sendNumeric(clientFd, 332, channelName + " :" + channel.topic);
        else
            sendNumeric(clientFd, 331, channelName + " :No topic is set");
        sendNamesReply(clientFd, channelName);
    }
}
