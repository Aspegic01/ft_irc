#include "Server.hpp"

void Server::handleNick(int clientFd, const ParsedCommand &command) {
    Client *client = clients[clientFd];

    if (command.params.empty()) {
        sendNumeric(clientFd, 431, ":No nickname given");
        return;
    }

    std::string newNick = command.params[0];
    if (!isNicknameValid(newNick)) {
        sendNumeric(clientFd, 432, newNick + " :Erroneous nickname");
        return;
    }

    std::map<std::string, int>::iterator nickIt = nickToFd.find(newNick);
    if (nickIt != nickToFd.end() && nickIt->second != clientFd) {
        sendNumeric(clientFd, 433, newNick + " :Nickname is already in use");
        return;
    }

    std::string oldNick = client->getNickname();
    if (!oldNick.empty())
        nickToFd.erase(oldNick);

    client->setNickname(newNick);
    nickToFd[newNick] = clientFd;

    if (!oldNick.empty()) {
        std::string nickMsg = ":" + oldNick + "!" + client->getUsername() + "@localhost NICK :" + newNick + "\r\n";
        for (std::map<int, Client *>::iterator it = clients.begin(); it != clients.end(); ++it)
            sendRaw(it->first, nickMsg);
    }

    maybeRegisterClient(clientFd);
}
