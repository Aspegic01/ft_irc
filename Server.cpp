#include "Server.hpp"
#include <cerrno>
#include <cstdlib>
#include <netinet/in.h>
#include <sstream>
#include <algorithm>

volatile sig_atomic_t Server::Signal = 0;

static std::vector<std::string> splitByComma(const std::string &value) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= value.size()) {
        size_t comma = value.find(',', start);
        if (comma == std::string::npos) {
            out.push_back(value.substr(start));
            break;
        }

        static std::vector<std::string> splitByCommaNonEmpty(const std::string &value) {
            std::vector<std::string> values = splitByComma(value);
            std::vector<std::string> out;
            for (size_t i = 0; i < values.size(); ++i) {
                if (!values[i].empty())
                    out.push_back(values[i]);
            }
            return out;
        }
        out.push_back(value.substr(start, comma - start));
        start = comma + 1;
    }
    return out;
}

static std::string toCode(int code) {
    std::ostringstream oss;
    if (code < 100)
        oss << '0';
    if (code < 10)
        oss << '0';
    oss << code;
    return oss.str();
}

void Server::signalHandler(int signum) {
    (void)signum;
    Signal = 1;
}

void Server::initServer(int port, const std::string &password) {
    this->port = port;
    this->password = password;
    setupServerSocket();
    std::cout << "Server started. Listening on port " << port << "...\n";
    runEventLoop();
}

void Server::setupServerSocket() {
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(this->port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    this->serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->serverFd == -1)
        throw std::runtime_error("Failed to create socket");

    int opt = 1;
    if (setsockopt(this->serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        throw std::runtime_error("Failed to set SO_REUSEADDR");

    if (fcntl(this->serverFd, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error("Failed to set O_NONBLOCK");

    if (bind(this->serverFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
        throw std::runtime_error("Failed to bind socket");

    if (listen(this->serverFd, SOMAXCONN) == -1)
        throw std::runtime_error("listen() failed");

    struct pollfd pfd;
    pfd.fd = this->serverFd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    this->fds.push_back(pfd);
}

void Server::runEventLoop() {
    while (Server::Signal == 0) {
        if (poll(&fds[0], fds.size(), -1) == -1) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("poll() failed");
        }

        std::vector<struct pollfd> current = fds;
        for (size_t i = 0; i < current.size(); ++i) {
            int fd = current[i].fd;
            short revents = current[i].revents;

            if (fd == serverFd) {
                if (revents & POLLIN)
                    acceptNewClient();
                continue;
            }

            if (clients.find(fd) == clients.end())
                continue;

            if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
                removeClient(fd, "Connection closed");
                continue;
            }
            if (revents & POLLIN)
                handleClientData(fd);
        }
    }
    closeAllConnections();
}

void Server::acceptNewClient() {
    struct sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);

    int clientFd = accept(this->serverFd, (sockaddr *)&clientAddr, &len);
    if (clientFd == -1)
        return;

    if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1) {
        close(clientFd);
        return;
    }

    Client *newClient = new Client();
    newClient->setFd(clientFd);
    newClient->setIpAdd(inet_ntoa(clientAddr.sin_addr));
    this->clients[clientFd] = newClient;

    struct pollfd pfd;
    pfd.fd = clientFd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    this->fds.push_back(pfd);

    std::cout << "Client <" << clientFd << "> connected.\n";
}

void Server::processBuffer(int clientFd) {
    std::string &buffer = this->clientBuffers[clientFd];
    size_t pos;

    while ((pos = buffer.find("\r\n")) != std::string::npos) {
        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 2);

        if (line.empty())
            continue;
        if (line.size() > 510)
            line = line.substr(0, 510);

        ParsedCommand command = Command::parse(line);
        if (!command.valid)
            continue;
        handleCommand(clientFd, command);

        if (clients.find(clientFd) == clients.end())
            return;
    }
}

void Server::handleClientData(int clientFd) {
    char buffer[1024];
    std::memset(buffer, 0, sizeof(buffer));

    ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead == 0) {
        removeClient(clientFd, "Client Quit");
        return;
    }
    if (bytesRead < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        removeClient(clientFd, "Read error");
        return;
    }

    buffer[bytesRead] = '\0';
    this->clientBuffers[clientFd].append(buffer);

    if (this->clientBuffers[clientFd].size() > 8192) {
        sendNumeric(clientFd, 417, ":Input line too long");
        removeClient(clientFd, "Excess flood");
        return;
    }

    processBuffer(clientFd);
}

void Server::sendRaw(int clientFd, const std::string &message) {
    if (clientFd < 0)
        return;
    send(clientFd, message.c_str(), message.size(), 0);
}

std::string Server::getClientTarget(int clientFd) const {
    std::map<int, Client *>::const_iterator it = clients.find(clientFd);
    if (it == clients.end() || it->second->getNickname().empty())
        return "*";
    return it->second->getNickname();
}

std::string Server::getClientPrefix(int clientFd) const {
    std::map<int, Client *>::const_iterator it = clients.find(clientFd);
    if (it == clients.end())
        return ":unknown!unknown@localhost";

    std::string nick = it->second->getNickname().empty() ? "*" : it->second->getNickname();
    std::string user = it->second->getUsername().empty() ? "unknown" : it->second->getUsername();
    return ":" + nick + "!" + user + "@localhost";
}

void Server::sendNumeric(int clientFd, int code, const std::string &message) {
    std::string reply = ":ircserv " + toCode(code) + " " + getClientTarget(clientFd) + " " + message + "\r\n";
    sendRaw(clientFd, reply);
}

bool Server::isNicknameValid(const std::string &nickname) const {
    if (nickname.empty())
        return false;
    if (!std::isalpha(nickname[0]) && std::string("[]\\`_^{|}").find(nickname[0]) == std::string::npos)
        return false;

    for (size_t i = 1; i < nickname.size(); ++i) {
        char c = nickname[i];
        if (!std::isalnum(c) && std::string("-[]\\`_^{|}").find(c) == std::string::npos)
            return false;
    }
    return true;
}

bool Server::isChannelNameValid(const std::string &channelName) const {
    if (channelName.size() < 2)
        return false;
    if (channelName[0] != '#')
        return false;
    for (size_t i = 1; i < channelName.size(); ++i) {
        char c = channelName[i];
        if (c == ' ' || c == ',' || c == 7)
            return false;
    }
    return true;
}

void Server::maybeRegisterClient(int clientFd) {
    std::map<int, Client *>::iterator it = clients.find(clientFd);
    if (it == clients.end())
        return;

    Client *client = it->second;
    if (client->getIsRegistered())
        return;

    if (client->getPassAccepted() && !client->getNickname().empty() && client->getUserSet()) {
        client->setIsRegistered(true);
        sendNumeric(clientFd, 1, ":Welcome to the Internet Relay Network " + getClientPrefix(clientFd).substr(1));
        sendNumeric(clientFd, 2, ":Your host is ircserv, running version 1.0");
        sendNumeric(clientFd, 3, ":This server was created today");
        sendNumeric(clientFd, 4, "ircserv 1.0 o itkol");
    }
}

void Server::handlePass(int clientFd, const ParsedCommand &command) {
    Client *client = clients[clientFd];

    if (client->getIsRegistered()) {
        sendNumeric(clientFd, 462, ":You may not reregister");
        return;
    }
    if (command.params.empty()) {
        sendNumeric(clientFd, 461, "PASS :Not enough parameters");
        return;
    }
    if (command.params[0] != password) {
        sendNumeric(clientFd, 464, ":Password incorrect");
        return;
    }

    client->setPassAccepted(true);
    maybeRegisterClient(clientFd);
}

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

void Server::handleUser(int clientFd, const ParsedCommand &command) {
    Client *client = clients[clientFd];

    if (client->getIsRegistered()) {
        sendNumeric(clientFd, 462, ":You may not reregister");
        return;
    }
    if (command.params.size() < 4) {
        sendNumeric(clientFd, 461, "USER :Not enough parameters");
        return;
    }

    client->setUsername(command.params[0]);
    client->setRealname(command.params[3]);
    client->setUserSet(true);

    maybeRegisterClient(clientFd);
}

void Server::handlePing(int clientFd, const ParsedCommand &command) {
    if (command.params.empty()) {
        sendNumeric(clientFd, 409, ":No origin specified");
        return;
    }
    sendRaw(clientFd, ":ircserv PONG ircserv :" + command.params[0] + "\r\n");
}

void Server::handlePong(int clientFd, const ParsedCommand &command) {
    (void)clientFd;
    (void)command;
}

void Server::removeClientFromChannel(int clientFd, const std::string &channelName, const std::string &reason, bool broadcastPart) {
    std::map<std::string, Channel>::iterator it = channels.find(channelName);
    if (it == channels.end())
        return;

    Channel &channel = it->second;
    if (channel.members.find(clientFd) == channel.members.end())
        return;

    if (broadcastPart) {
        std::string partMsg = getClientPrefix(clientFd) + " PART " + channelName;
        if (!reason.empty())
            partMsg += " :" + reason;
        partMsg += "\r\n";
        broadcastToChannel(channelName, partMsg, -1);
    }

    channel.members.erase(clientFd);
    channel.operators.erase(clientFd);
    channel.invited.erase(clientFd);

    if (!channel.members.empty() && channel.operators.empty())
        channel.operators.insert(*channel.members.begin());

    if (channel.members.empty())
        channels.erase(it);
}

void Server::removeClient(int clientFd, const std::string &quitMessage) {
    std::map<int, Client *>::iterator clIt = clients.find(clientFd);
    if (clIt == clients.end())
        return;

    std::string quitMsg = getClientPrefix(clientFd) + " QUIT :" + quitMessage + "\r\n";
    std::set<int> notified;

    for (std::map<std::string, Channel>::iterator it = channels.begin(); it != channels.end(); ++it) {
        if (it->second.members.find(clientFd) == it->second.members.end())
            continue;
        for (std::set<int>::iterator m = it->second.members.begin(); m != it->second.members.end(); ++m) {
            if (*m != clientFd)
                notified.insert(*m);
        }
    }

    for (std::set<int>::iterator it = notified.begin(); it != notified.end(); ++it)
        sendRaw(*it, quitMsg);

    std::vector<std::string> channelNames;
    for (std::map<std::string, Channel>::iterator it = channels.begin(); it != channels.end(); ++it) {
        if (it->second.members.find(clientFd) != it->second.members.end())
            channelNames.push_back(it->first);
    }
    for (size_t i = 0; i < channelNames.size(); ++i)
        removeClientFromChannel(clientFd, channelNames[i], "", false);

    std::string nick = clIt->second->getNickname();
    if (!nick.empty())
        nickToFd.erase(nick);

    close(clientFd);

    for (size_t i = 0; i < fds.size(); ++i) {
        if (fds[i].fd == clientFd) {
            fds.erase(fds.begin() + i);
            break;
        }
    }

    delete clIt->second;
    clients.erase(clIt);
    clientBuffers.erase(clientFd);

    std::cout << "Client <" << clientFd << "> disconnected.\n";
}

void Server::broadcastToChannel(const std::string &channelName, const std::string &message, int excludeFd) {
    std::map<std::string, Channel>::iterator it = channels.find(channelName);
    if (it == channels.end())
        return;

    for (std::set<int>::iterator member = it->second.members.begin(); member != it->second.members.end(); ++member) {
        if (*member == excludeFd)
            continue;
        sendRaw(*member, message);
    }
}

void Server::sendNamesReply(int clientFd, const std::string &channelName) {
    std::map<std::string, Channel>::iterator it = channels.find(channelName);
    if (it == channels.end())
        return;

    std::string names;
    Channel &channel = it->second;
    for (std::set<int>::iterator member = channel.members.begin(); member != channel.members.end(); ++member) {
        std::map<int, Client *>::iterator cl = clients.find(*member);
        if (cl == clients.end())
            continue;
        if (!names.empty())
            names += " ";
        if (channel.operators.find(*member) != channel.operators.end())
            names += "@";
        names += cl->second->getNickname();
    }

    sendNumeric(clientFd, 353, "= " + channelName + " :" + names);
    sendNumeric(clientFd, 366, channelName + " :End of /NAMES list");
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

void Server::handleInvite(int clientFd, const ParsedCommand &command) {
    if (command.params.size() < 2) {
        sendNumeric(clientFd, 461, "INVITE :Not enough parameters");
        return;
    }

    std::string nick = command.params[0];
    std::string channelName = command.params[1];
    if (!isChannelNameValid(channelName)) {
        sendNumeric(clientFd, 403, channelName + " :No such channel");
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

    std::map<std::string, int>::iterator nickIt = nickToFd.find(nick);
    if (nickIt == nickToFd.end()) {
        sendNumeric(clientFd, 401, nick + " :No such nick/channel");
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

void Server::handleKick(int clientFd, const ParsedCommand &command) {
    if (command.params.size() < 2) {
        sendNumeric(clientFd, 461, "KICK :Not enough parameters");
        return;
    }

    std::vector<std::string> channelNames = splitByCommaNonEmpty(command.params[0]);
    std::vector<std::string> nickNames = splitByCommaNonEmpty(command.params[1]);
    std::string reason = command.params.size() > 2 ? command.params[2] : clients[clientFd]->getNickname();

    if (channelNames.empty() || nickNames.empty()) {
        sendNumeric(clientFd, 461, "KICK :Not enough parameters");
        return;
    }

    for (size_t i = 0; i < channelNames.size(); ++i) {
        const std::string &channelName = channelNames[i];
        if (!isChannelNameValid(channelName)) {
            sendNumeric(clientFd, 403, channelName + " :No such channel");
            continue;
        }

        std::map<std::string, Channel>::iterator chIt = channels.find(channelName);
        if (chIt == channels.end()) {
            sendNumeric(clientFd, 403, channelName + " :No such channel");
            continue;
        }

        Channel &channel = chIt->second;
        if (channel.members.find(clientFd) == channel.members.end()) {
            sendNumeric(clientFd, 442, channelName + " :You're not on that channel");
            continue;
        }
        if (channel.operators.find(clientFd) == channel.operators.end()) {
            sendNumeric(clientFd, 482, channelName + " :You're not channel operator");
            continue;
        }

        std::vector<std::string> targets;
        if (nickNames.size() == 1 || i >= nickNames.size())
            targets.push_back(nickNames[0]);
        else if (channelNames.size() == nickNames.size())
            targets.push_back(nickNames[i]);
        else if (channelNames.size() == 1)
            targets = nickNames;
        else
            targets.push_back(nickNames[0]);

        for (size_t j = 0; j < targets.size(); ++j) {
            const std::string &nick = targets[j];
            std::map<std::string, int>::iterator nickIt = nickToFd.find(nick);
            if (nickIt == nickToFd.end() || channel.members.find(nickIt->second) == channel.members.end()) {
                sendNumeric(clientFd, 441, nick + " " + channelName + " :They aren't on that channel");
                continue;
            }

            std::string kickMsg = getClientPrefix(clientFd) + " KICK " + channelName + " " + nick + " :" + reason + "\r\n";
            broadcastToChannel(channelName, kickMsg, -1);
            removeClientFromChannel(nickIt->second, channelName, "", false);
        }
    }
}

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

void Server::handleQuit(int clientFd, const ParsedCommand &command) {
    std::string reason = "Client Quit";
    if (!command.params.empty())
        reason = command.params[0];
    removeClient(clientFd, reason);
}

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

void Server::closeAllConnections() {
    if (closed)
        return;
    closed = true;

    std::vector<int> allClients;
    for (std::map<int, Client *>::iterator it = clients.begin(); it != clients.end(); ++it)
        allClients.push_back(it->first);

    for (size_t i = 0; i < allClients.size(); ++i) {
        sendRaw(allClients[i], ":ircserv ERROR :Server shutting down\r\n");
        removeClient(allClients[i], "Server shutting down");
    }

    if (this->serverFd != -1) {
        close(this->serverFd);
        this->serverFd = -1;
    }

    fds.clear();
    channels.clear();
    clientBuffers.clear();
    nickToFd.clear();
}

Server::Server() : port(0), password(""), serverFd(-1), closed(false) {
}

Server::~Server() {
    closeAllConnections();
}
