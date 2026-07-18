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

void Server::handlePart(int clientFd, const ParsedCommand &command) {
    if (command.params.empty()) {
        sendNumeric(clientFd, 461, "PART :Not enough parameters");
        return;
    }

    std::vector<std::string> channelNames = splitByComma(command.params[0]);
    std::string reason = (command.params.size() > 1) ? command.params[1] : "";

    for (size_t i = 0; i < channelNames.size(); ++i) {
        const std::string &channelName = channelNames[i];

        std::map<std::string, Channel>::iterator it = channels.find(channelName);
        if (it == channels.end()) {
            sendNumeric(clientFd, 403, channelName + " :No such channel");
            continue;
        }
        if (it->second.members.find(clientFd) == it->second.members.end()) {
            sendNumeric(clientFd, 442, channelName + " :You're not on that channel");
            continue;
        }

        removeClientFromChannel(clientFd, channelName, reason, true);
    }
}
