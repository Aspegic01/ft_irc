#include "Server.hpp"

void Server::handlePrivmsg(int clientFd, const ParsedCommand &command, bool notice) {
    if (command.params.empty()) {
        if (!notice)
            sendNumeric(clientFd, 411, ":No recipient given (PRIVMSG)");
        return;
    }
    if (command.params.size() < 2) {
        if (!notice)
            sendNumeric(clientFd, 412, ":No text to send");
        return;
    }

    std::string target = command.params[0];
    std::string text = command.params[1];
    std::string cmd = notice ? "NOTICE" : "PRIVMSG";
    std::string out = getClientPrefix(clientFd) + " " + cmd + " " + target + " :" + text + "\r\n";

    if (!target.empty() && target[0] == '#') {
        std::map<std::string, Channel>::iterator it = channels.find(target);
        if (it == channels.end()) {
            if (!notice)
                sendNumeric(clientFd, 403, target + " :No such channel");
            return;
        }
        if (it->second.members.find(clientFd) == it->second.members.end()) {
            if (!notice)
                sendNumeric(clientFd, 442, target + " :You're not on that channel");
            return;
        }
        broadcastToChannel(target, out, clientFd);
        return;
    }

    std::map<std::string, int>::iterator nickIt = nickToFd.find(target);
    if (nickIt == nickToFd.end()) {
        if (!notice)
            sendNumeric(clientFd, 401, target + " :No such nick/channel");
        return;
    }
    sendRaw(nickIt->second, out);
}
