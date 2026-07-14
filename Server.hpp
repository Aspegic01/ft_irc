#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <stdexcept>
#include <cstring>
#include <csignal>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include "Client.hpp"
#include "Command.hpp"

class Server {
private:
    struct Channel {
        std::string name;
        std::string topic;
        std::string key;
        int userLimit;
        bool inviteOnly;
        bool topicRestricted;
        std::set<int> members;
        std::set<int> operators;
        std::set<int> invited;

        Channel()
            : name(""), topic(""), key(""), userLimit(-1), inviteOnly(false), topicRestricted(false) {}
    };

    int port;
    std::string password;
    int serverFd;
    bool closed;

    std::vector<struct pollfd> fds;
    std::map<int, Client*> clients;
    std::map<int, std::string> clientBuffers;
    std::map<std::string, int> nickToFd;
    std::map<std::string, Channel> channels;

    void setupServerSocket();
    void runEventLoop();
    void acceptNewClient();
    void handleClientData(int clientFd);
    void processBuffer(int clientFd);
    void handleCommand(int clientFd, const ParsedCommand &command);

    void removeClient(int clientFd, const std::string &quitMessage);
    void closeAllConnections();

    void sendRaw(int clientFd, const std::string &message);
    void sendNumeric(int clientFd, int code, const std::string &message);
    void maybeRegisterClient(int clientFd);

    std::string getClientTarget(int clientFd) const;
    std::string getClientPrefix(int clientFd) const;
    bool isNicknameValid(const std::string &nickname) const;
    bool isChannelNameValid(const std::string &channelName) const;

    void broadcastToChannel(const std::string &channelName, const std::string &message, int excludeFd);
    void sendNamesReply(int clientFd, const std::string &channelName);

    void handlePass(int clientFd, const ParsedCommand &command);
    void handleNick(int clientFd, const ParsedCommand &command);
    void handleUser(int clientFd, const ParsedCommand &command);
    void handlePing(int clientFd, const ParsedCommand &command);
    void handlePong(int clientFd, const ParsedCommand &command);
    void handleQuit(int clientFd, const ParsedCommand &command);
    void handleJoin(int clientFd, const ParsedCommand &command);
    void handlePart(int clientFd, const ParsedCommand &command);
    void handlePrivmsg(int clientFd, const ParsedCommand &command, bool notice);
    void handleTopic(int clientFd, const ParsedCommand &command);
    void handleInvite(int clientFd, const ParsedCommand &command);
    void handleKick(int clientFd, const ParsedCommand &command);
    void handleMode(int clientFd, const ParsedCommand &command);

    void removeClientFromChannel(int clientFd, const std::string &channelName, const std::string &reason, bool broadcastPart);
    std::string channelModes(const Channel &channel) const;

    Server(const Server &other);
    Server &operator=(const Server &other);

public:
    Server();
    ~Server();

    static volatile sig_atomic_t Signal;
    static void signalHandler(int signum);

    void initServer(int port, const std::string &password);
};

#endif
