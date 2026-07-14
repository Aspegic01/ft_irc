#include "Client.hpp"

Client::Client()
    : fd(-1), ipAdd(""), nickname(""), username(""), realname(""),
      passAccepted(false), userSet(false), isRegistered(false) {
}

Client::Client(const Client &other) {
    *this = other;
}

Client &Client::operator=(const Client &other) {
    if (this != &other) {
        this->fd = other.fd;
        this->ipAdd = other.ipAdd;
        this->nickname = other.nickname;
        this->username = other.username;
        this->realname = other.realname;
        this->passAccepted = other.passAccepted;
        this->userSet = other.userSet;
        this->isRegistered = other.isRegistered;
    }
    return *this;
}

Client::~Client() {
}

int Client::getFd() const { return this->fd; }
std::string Client::getIpAdd() const { return this->ipAdd; }
std::string Client::getNickname() const { return this->nickname; }
std::string Client::getUsername() const { return this->username; }
std::string Client::getRealname() const { return this->realname; }
bool Client::getPassAccepted() const { return this->passAccepted; }
bool Client::getUserSet() const { return this->userSet; }
bool Client::getIsRegistered() const { return this->isRegistered; }

void Client::setFd(int fd) { this->fd = fd; }
void Client::setIpAdd(const std::string &ipAdd) { this->ipAdd = ipAdd; }
void Client::setNickname(const std::string &nickname) { this->nickname = nickname; }
void Client::setUsername(const std::string &username) { this->username = username; }
void Client::setRealname(const std::string &realname) { this->realname = realname; }
void Client::setPassAccepted(bool value) { this->passAccepted = value; }
void Client::setUserSet(bool value) { this->userSet = value; }
void Client::setIsRegistered(bool status) { this->isRegistered = status; }
