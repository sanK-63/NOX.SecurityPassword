#include "core/StatelessGenerator.h"
#include <sodium.h>
#include <stdexcept>
#include <vector>
#include <cstring>

std::string StatelessGenerator::generate(
    const std::string& masterPassword,
    const std::string& domain,
    const std::string& login,
    size_t passwordLength,
    const std::string& alphabet)
{
    if (alphabet.empty()) {
        throw std::invalid_argument("alphabet must not be empty");
    }

    std::vector<unsigned char> salt(crypto_pwhash_SALTBYTES);
    std::string saltInput = domain + "|" + login;
    crypto_generichash(
        salt.data(), salt.size(),
        reinterpret_cast<const unsigned char*>(saltInput.data()),
        saltInput.size(),
        nullptr, 0);

    std::vector<unsigned char> rawKey(passwordLength);

    if (crypto_pwhash(
            rawKey.data(), rawKey.size(),
            masterPassword.data(), masterPassword.size(),
            salt.data(),
            crypto_pwhash_OPSLIMIT_SENSITIVE,
            crypto_pwhash_MEMLIMIT_SENSITIVE,
            crypto_pwhash_ALG_ARGON2ID13) != 0) {
        throw std::runtime_error("Argon2id failed (insufficient memory)");
    }

    sodium_memzero(salt.data(), salt.size());

    std::string password;
    password.reserve(passwordLength);

    size_t alphabetSize = alphabet.size();
    size_t maxValid = 256 - (256 % alphabetSize);

    for (size_t i = 0; i < passwordLength; ++i) {
        unsigned char byte = rawKey[i];
        if (byte >= maxValid) {
            size_t extra = 0;
            while (byte >= maxValid) {
                extra++;
                byte = rawKey[(i + extra) % passwordLength];
            }
            i += extra;
            if (i >= passwordLength) break;
        }
        password += alphabet[byte % alphabetSize];
    }

    while (password.size() < passwordLength) {
        password += alphabet[rawKey[password.size() % passwordLength] % alphabetSize];
    }

    sodium_memzero(rawKey.data(), rawKey.size());

    return password;
}
