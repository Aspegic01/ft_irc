#include "Client.hpp"

// -------------------------------------------------------------
// Orthodox Canonical Form implementation
// -------------------------------------------------------------
Client::Client() : fd(-1), ipAdd(""), nickname(""), username(""), isRegistered(false) {
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
        this->isRegistered = other.isRegistered;
    }
    return *this;
}

Client::~Client() {
    // No dynamic memory to clean up directly in this class
}

// -------------------------------------------------------------
// Getters
// -------------------------------------------------------------
int Client::getFd() const {
    return this->fd;
}

std::string Client::getIpAdd() const {
    return this->ipAdd;
}

std::string Client::getNickname() const {
    return this->nickname;
}

std::string Client::getUsername() const {
    return this->username;
}

bool Client::getIsRegistered() const {
    return this->isRegistered;
}

// -------------------------------------------------------------
// Setters
// -------------------------------------------------------------
void Client::setFd(int fd) {
    this->fd = fd;
}

void Client::setIpAdd(const std::string &ipAdd) {
    this->ipAdd = ipAdd;
}

void Client::setNickname(const std::string &nickname) {
    this->nickname = nickname;
}

void Client::setUsername(const std::string &username) {
    this->username = username;
}

void Client::setIsRegistered(bool status) {
    this->isRegistered = status;
}