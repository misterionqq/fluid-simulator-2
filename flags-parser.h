#pragma once

#include <string>
#include <unordered_map>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include "fluid-creator.h"


class parser {
public:
    explicit parser(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            option(argv[i]);
        }
    }

    std::string get_option(const std::string& option) const {
        auto it = comp_options.find(option);
        if (it == comp_options.end()) {
            throw std::invalid_argument("Option not found: " + option);
        }
        return it->second;
    }

private:
    void option(const std::string& optionString) {
        auto delimiterPos = optionString.find('=');
        if (delimiterPos == std::string::npos) {
            throw std::invalid_argument("Invalid option format: " + optionString);
        }

        std::string key = optionString.substr(0, delimiterPos);
        std::string value = optionString.substr(delimiterPos + 1);
        comp_options.emplace(std::move(key), std::move(value));
    }

    std::unordered_map<std::string, std::string> comp_options;
};

bool parse_type(const std::string& typePrefix, const std::string& typeName, int& param1, int& param2) {
    std::string pattern = typePrefix + "(%d,%d)";
    return sscanf(typeName.c_str(), pattern.c_str(), &param1, &param2) == 2;
}

int get_type(const std::string& typeName) {
    int param1 = 0, param2 = 0;
    if (parse_type("FIXED", typeName, param1, param2)) {
        return FIXED(param1, param2);
    }
    if (parse_type("FAST_FIXED", typeName, param1, param2)) {
        return FAST_FIXED(param1, param2);
    }
    if (typeName == "DOUBLE") {
        return DOUBLE;
    }
    if (typeName == "FLOAT") {
        return FLOAT;
    }

    throw std::invalid_argument("Unknown type: " + typeName);
}
