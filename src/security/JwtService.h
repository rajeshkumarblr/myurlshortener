#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

class JwtService {
public:
    struct Claims {
        long userId{0};
        std::string name;
        std::string email;
        std::chrono::system_clock::time_point expiresAt;
    };

    JwtService(std::string secret, std::chrono::seconds ttl);

    std::string issueToken(long userId,
                           std::string_view name,
                           std::string_view email) const;

    std::optional<Claims> verify(const std::string& token) const;

private:
    std::string secret_;
    std::chrono::seconds ttl_;

    static std::string base64UrlEncode(const std::string& data);
    static std::optional<std::string> base64UrlDecode(const std::string& input);
    std::string hmacSha256(const std::string& data) const;
};
