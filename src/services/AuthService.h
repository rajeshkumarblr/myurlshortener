#pragma once
#include "DataStore.h"
#include "ServiceError.h"
#include <drogon/HttpRequest.h>
#include <memory>
#include <optional>
#include <string>

class AuthService {
public:
    struct UserContext {
        long id{0};
        std::string name;
        std::string email;
    };

    struct LoginResult {
        UserContext user;
        std::string token;
    };

    explicit AuthService(std::shared_ptr<DataStore> store);

    LoginResult registerUser(const std::string& name,
                             const std::string& email,
                             const std::string& password);

    LoginResult login(const std::string& email,
                      const std::string& password);

    std::optional<UserContext> authenticate(
        const drogon::HttpRequestPtr& request,
        std::string* rawToken = nullptr) const;

private:
    std::shared_ptr<DataStore> store_;

    static std::string normalizeEmail(std::string email);
    static std::string trim(std::string value);
    static void validatePassword(const std::string& password);
    static void ensureEmailAndPassword(const std::string& email,
                                       const std::string& password);
    static void ensureName(const std::string& name);

    std::optional<std::string> extractToken(
        const drogon::HttpRequestPtr& request) const;
};
