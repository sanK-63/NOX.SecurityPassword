#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class StatelessGenerator {
public:
    std::string generate(
        const std::string& masterPassword,
        const std::string& domain,
        const std::string& login,
        size_t passwordLength,
        const std::string& alphabet);
};
