#include "Command.hpp"
#include <cctype>

std::string Command::toUpper(const std::string &value) {
    std::string out = value;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(std::toupper(out[i]));
    return out;
}

ParsedCommand Command::parse(const std::string &line) {
    ParsedCommand parsed;
    if (line.empty())
        return parsed;

    std::string input = line;
    size_t pos = 0;

    if (!input.empty() && input[0] == ':') {
        size_t spacePos = input.find(' ');
        if (spacePos == std::string::npos)
            return parsed;
        parsed.prefix = input.substr(1, spacePos - 1);
        pos = spacePos + 1;
        while (pos < input.size() && input[pos] == ' ')
            ++pos;
    }

    size_t commandStart = pos;
    while (pos < input.size() && input[pos] != ' ')
        ++pos;
    if (commandStart == pos)
        return parsed;

    parsed.command = toUpper(input.substr(commandStart, pos - commandStart));

    while (pos < input.size()) {
        while (pos < input.size() && input[pos] == ' ')
            ++pos;
        if (pos >= input.size())
            break;

        if (input[pos] == ':') {
            parsed.params.push_back(input.substr(pos + 1));
            break;
        }

        size_t nextSpace = input.find(' ', pos);
        if (nextSpace == std::string::npos) {
            parsed.params.push_back(input.substr(pos));
            break;
        }

        parsed.params.push_back(input.substr(pos, nextSpace - pos));
        pos = nextSpace + 1;
    }

    parsed.valid = true;
    return parsed;
}
