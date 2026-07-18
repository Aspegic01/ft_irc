#include "Server.hpp"
#include <cstdlib>
#include <sstream>

std::string Server::channelModes(const Channel &channel) const {
    std::string modes = "+";
    if (channel.inviteOnly)
        modes += "i";
    if (channel.topicRestricted)
        modes += "t";
    if (!channel.key.empty())
        modes += "k";
    if (channel.userLimit >= 0)
        modes += "l";
    return modes;
}

void Server::handleMode(int clientFd, const ParsedCommand &command) {
    if (command.params.empty()) {
        sendNumeric(clientFd, 461, "MODE :Not enough parameters");
        return;
    }

    std::string target = command.params[0];
    if (target.empty() || target[0] != '#') {
        sendNumeric(clientFd, 501, ":Unknown MODE flag");
        return;
    }

    std::map<std::string, Channel>::iterator chIt = channels.find(target);
    if (chIt == channels.end()) {
        sendNumeric(clientFd, 403, target + " :No such channel");
        return;
    }

    Channel &channel = chIt->second;
    if (command.params.size() == 1) {
        std::string modes = channelModes(channel);
        std::string params;
        if (!channel.key.empty())
            params += " " + channel.key;
        if (channel.userLimit >= 0) {
            std::ostringstream oss;
            oss << channel.userLimit;
            params += " " + oss.str();
        }
        sendNumeric(clientFd, 324, target + " " + modes + params);
        return;
    }

    if (channel.members.find(clientFd) == channel.members.end()) {
        sendNumeric(clientFd, 442, target + " :You're not on that channel");
        return;
    }
    if (channel.operators.find(clientFd) == channel.operators.end()) {
        sendNumeric(clientFd, 482, target + " :You're not channel operator");
        return;
    }

    std::string flags = command.params[1];
    bool adding = true;
    size_t argIndex = 2;
    std::string applied = "+";
    std::vector<std::string> appliedParams;

    for (size_t i = 0; i < flags.size(); ++i) {
        char flag = flags[i];
        if (flag == '+') {
            adding = true;
            continue;
        }
        if (flag == '-') {
            adding = false;
            continue;
        }

        if (flag == 'i') {
            channel.inviteOnly = adding;
            applied += flag;
        } else if (flag == 't') {
            channel.topicRestricted = adding;
            applied += flag;
        } else if (flag == 'k') {
            if (adding) {
                if (argIndex >= command.params.size()) continue;
                channel.key = command.params[argIndex++];
                applied += flag;
                appliedParams.push_back(channel.key);
            } else {
                channel.key = "";
                applied += flag;
            }
        } else if (flag == 'l') {
            if (adding) {
                if (argIndex >= command.params.size()) continue;
                channel.userLimit = std::atoi(command.params[argIndex].c_str());
                if (channel.userLimit < 0)
                    channel.userLimit = -1;
                ++argIndex;
                applied += flag;
                std::ostringstream oss;
                oss << channel.userLimit;
                appliedParams.push_back(oss.str());
            } else {
                channel.userLimit = -1;
                applied += flag;
            }
        } else if (flag == 'o') {
            if (argIndex >= command.params.size()) continue;
            std::string nick = command.params[argIndex++];
            std::map<std::string, int>::iterator nickIt = nickToFd.find(nick);
            if (nickIt == nickToFd.end() || channel.members.find(nickIt->second) == channel.members.end())
                continue;
            if (adding)
                channel.operators.insert(nickIt->second);
            else
                channel.operators.erase(nickIt->second);
            applied += flag;
            appliedParams.push_back(nick);
        } else {
            sendNumeric(clientFd, 472, std::string(1, flag) + " :is unknown mode char to me");
        }
    }

    if (applied.size() > 1) {
        std::string modeMsg = getClientPrefix(clientFd) + " MODE " + target + " " + applied;
        for (size_t i = 0; i < appliedParams.size(); ++i)
            modeMsg += " " + appliedParams[i];
        modeMsg += "\r\n";
        broadcastToChannel(target, modeMsg, -1);
    }
}
