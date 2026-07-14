#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <string>
#include <vector>

struct ParsedCommand {
    std::string prefix;
    std::string command;
    std::vector<std::string> params;
    bool valid;

    ParsedCommand() : valid(false) {}
};

class Command {
public:
    static ParsedCommand parse(const std::string &line);
    static std::string toUpper(const std::string &value);
};

#endif
