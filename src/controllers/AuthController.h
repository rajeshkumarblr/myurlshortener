#pragma once
#include "../services/AuthService.h"
#include <drogon/HttpController.h>
#include <functional>
#include <memory>

class AuthController {
public:
    explicit AuthController(std::shared_ptr<AuthService> authService);

    void handleRegister(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void handleLogin(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& cb);

private:
    std::shared_ptr<AuthService> authService_;

    drogon::HttpResponsePtr validationError(const std::string& message) const;
    drogon::HttpResponsePtr errorResponse(drogon::HttpStatusCode status,
                                          const std::string& message) const;
    drogon::HttpResponsePtr successResponse(const AuthService::LoginResult& result) const;
};
