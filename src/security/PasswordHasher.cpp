#include "PasswordHasher.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <stdexcept>

namespace {
constexpr size_t kSaltBytes = 16;
constexpr size_t kKeyBytes = 32;
constexpr int kIterations = 120000;

std::vector<unsigned char> fromHex(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("invalid hex input");
    }
    std::vector<unsigned char> data(hex.size() / 2);
    for (size_t i = 0; i < data.size(); ++i) {
        unsigned int byte;
        std::stringstream ss;
        ss << std::hex << hex.substr(i * 2, 2);
        ss >> byte;
        data[i] = static_cast<unsigned char>(byte);
    }
    return data;
}
}

std::string PasswordHasher::hash(const std::string& password) {
    const auto saltHex = randomSalt();
    const auto keyHex = deriveKey(password, saltHex);
    return saltHex + ":" + keyHex;
}

bool PasswordHasher::verify(const std::string& password,
                            const std::string& storedHash) {
    const auto delim = storedHash.find(':');
    if (delim == std::string::npos) {
        return false;
    }
    auto saltHex = storedHash.substr(0, delim);
    auto expectedHex = storedHash.substr(delim + 1);
    auto derivedHex = deriveKey(password, saltHex);
    if (derivedHex.size() != expectedHex.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (size_t i = 0; i < derivedHex.size(); ++i) {
        diff |= static_cast<unsigned char>(derivedHex[i] ^ expectedHex[i]);
    }
    return diff == 0;
}

std::string PasswordHasher::toHex(const unsigned char* data, size_t length) {
    std::ostringstream oss;
    for (size_t i = 0; i < length; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::string PasswordHasher::randomSalt() {
    unsigned char buffer[kSaltBytes];
    if (RAND_bytes(buffer, sizeof(buffer)) != 1) {
        throw std::runtime_error("unable to generate salt");
    }
    return toHex(buffer, sizeof(buffer));
}

std::string PasswordHasher::deriveKey(const std::string& password,
                                      const std::string& saltHex) {
    auto saltBytes = fromHex(saltHex);
    unsigned char key[kKeyBytes];
    if (PKCS5_PBKDF2_HMAC(password.c_str(),
                           static_cast<int>(password.size()),
                           saltBytes.data(),
                           static_cast<int>(saltBytes.size()),
                           kIterations,
                           EVP_sha256(),
                           sizeof(key),
                           key) != 1) {
        throw std::runtime_error("unable to hash password");
    }
    return toHex(key, sizeof(key));
}
