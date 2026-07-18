#include "Server.hpp"
#include <cerrno>
#include <sstream>
#include <algorithm>

volatile sig_atomic_t Server::Signal = 0;

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
