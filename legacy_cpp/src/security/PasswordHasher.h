#pragma once
#include <string>

class PasswordHasher {
public:
    static std::string hash(const std::string& password);
    static bool verify(const std::string& password, const std::string& storedHash);

private:
    static std::string toHex(const unsigned char* data, size_t length);
    static std::string randomSalt();
    static std::string deriveKey(const std::string& password,
                                 const std::string& saltHex);
};
