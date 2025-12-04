#include "AuthService.h"
#include "ServiceError.h"
#include "../security/JwtService.h"
#include "../security/PasswordHasher.h"
#include <drogon/drogon.h>
#include <algorithm>
#include <cctype>

using drogon::HttpStatusCode;

AuthService::AuthService(std::shared_ptr<DataStore> store,
                         std::shared_ptr<JwtService> jwtService)
    : store_(std::move(store)),
      jwtService_(std::move(jwtService)) {
    if (!store_) {
        throw std::runtime_error("DataStore dependency missing");
    }
    if (!jwtService_) {
        throw std::runtime_error("JwtService dependency missing");
    }
}

AuthService::LoginResult AuthService::registerUser(const std::string& name,
                                                    const std::string& email,
                                                    const std::string& password) {
    ensureName(name);
    ensureEmailAndPassword(email, password);
    auto normalizedEmail = normalizeEmail(email);
    validatePassword(password);

    if (store_->findUserByEmail(normalizedEmail)) {
        throw ServiceError(HttpStatusCode::k409Conflict, "email already exists");
    }

    auto cleanedName = trim(name);
    auto hashed = PasswordHasher::hash(password);
    auto userId = store_->createUser(cleanedName, normalizedEmail, hashed);
    UserContext ctx{userId, cleanedName, normalizedEmail};
    auto token = jwtService_->issueToken(userId, ctx.name, ctx.email);

    return LoginResult{ctx, token};
}

AuthService::LoginResult AuthService::login(const std::string& email,
                                            const std::string& password) {
    ensureEmailAndPassword(email, password);
    auto normalized = normalizeEmail(email);
    auto user = store_->findUserByEmail(normalized);
    if (!user) {
        throw ServiceError(HttpStatusCode::k401Unauthorized, "invalid credentials");
    }
    if (!PasswordHasher::verify(password, user->passwordHash)) {
        throw ServiceError(HttpStatusCode::k401Unauthorized, "invalid credentials");
    }
    auto token = jwtService_->issueToken(user->id, user->name, user->email);
    UserContext ctx{user->id, user->name, user->email};
    return LoginResult{ctx, token};
}

std::optional<AuthService::UserContext> AuthService::authenticate(
    const drogon::HttpRequestPtr& request,
    std::string* rawToken) const {
    auto token = extractToken(request);
    if (!token) {
        return std::nullopt;
    }
    auto claims = jwtService_->verify(*token);
    if (!claims) {
        return std::nullopt;
    }
    if (rawToken) {
        *rawToken = *token;
    }
    return UserContext{claims->userId, claims->name, claims->email};
}

std::string AuthService::normalizeEmail(std::string email) {
    auto trimmed = trim(std::move(email));
    std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return trimmed;
}

std::string AuthService::trim(std::string value) {
    auto notSpace = [](int ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

void AuthService::validatePassword(const std::string& password) {
    if (password.size() < 8) {
        throw ServiceError(HttpStatusCode::k400BadRequest,
                           "password must be at least 8 characters");
    }
}

void AuthService::ensureEmailAndPassword(const std::string& email,
                                         const std::string& password) {
    if (trim(email).empty()) {
        throw ServiceError(HttpStatusCode::k400BadRequest, "email required");
    }
    if (trim(password).empty()) {
        throw ServiceError(HttpStatusCode::k400BadRequest, "password required");
    }
}

void AuthService::ensureName(const std::string& name) {
    if (trim(name).empty()) {
        throw ServiceError(HttpStatusCode::k400BadRequest, "name required");
    }
}

std::optional<std::string> AuthService::extractToken(
    const drogon::HttpRequestPtr& request) const {
    auto header = request->getHeader("authorization");
    if (!header.empty()) {
        if (header.rfind("Bearer ", 0) == 0) {
            return header.substr(7);
        }
        if (header.rfind("Token ", 0) == 0) {
            return header.substr(6);
        }
        return header;
    }
    auto apiKey = request->getHeader("x-api-key");
    if (!apiKey.empty()) {
        return apiKey;
    }
    apiKey = request->getHeader("x-api-token");
    if (!apiKey.empty()) {
        return apiKey;
    }
    auto queryToken = request->getParameter("api_key");
    if (!queryToken.empty()) {
        return queryToken;
    }
    return std::nullopt;
}
