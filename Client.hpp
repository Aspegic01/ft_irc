#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>
#include <string>

class Client {
private:
    // Network information
    int         fd;
    std::string ipAdd;
    
    // IRC user information
    std::string nickname;
    std::string username;
    
    // Registration state (becomes true after valid PASS, NICK, and USER)
    bool        isRegistered; 

public:
    // -------------------------------------------------------------
    // Orthodox Canonical Form
    // -------------------------------------------------------------
    Client();
    Client(const Client &other);
    Client &operator=(const Client &other);
    ~Client();

    // -------------------------------------------------------------
    // Getters
    // -------------------------------------------------------------
    int         getFd() const;
    std::string getIpAdd() const;
    std::string getNickname() const;
    std::string getUsername() const;
    bool        getIsRegistered() const;

    // -------------------------------------------------------------
    // Setters
    // -------------------------------------------------------------
    void setFd(int fd);
    void setIpAdd(const std::string &ipAdd);
    void setNickname(const std::string &nickname);
    void setUsername(const std::string &username);
    void setIsRegistered(bool status);
};

#endif // CLIENT_HPP