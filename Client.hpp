#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client {
private:
    int         fd;
    std::string ipAdd;
    std::string nickname;
    std::string username;
    std::string realname;
    bool        passAccepted;
    bool        userSet;
    bool        isRegistered;

public:
    Client();
    Client(const Client &other);
    Client &operator=(const Client &other);
    ~Client();

    int         getFd() const;
    std::string getIpAdd() const;
    std::string getNickname() const;
    std::string getUsername() const;
    std::string getRealname() const;
    bool        getPassAccepted() const;
    bool        getUserSet() const;
    bool        getIsRegistered() const;

    void setFd(int fd);
    void setIpAdd(const std::string &ipAdd);
    void setNickname(const std::string &nickname);
    void setUsername(const std::string &username);
    void setRealname(const std::string &realname);
    void setPassAccepted(bool value);
    void setUserSet(bool value);
    void setIsRegistered(bool status);
};

#endif
