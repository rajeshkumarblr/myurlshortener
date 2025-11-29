#include "AuthController.h"
#include "../services/ServiceError.h"
#include <drogon/drogon.h>
#include <json/json.h>

AuthController::AuthController(std::shared_ptr<AuthService> authService)
    : authService_(std::move(authService)) {}

void AuthController::handleRegister(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    if (!body) {
        cb(validationError("invalid JSON payload"));
        return;
    }
    if (!body->isMember("name") || !(*body)["name"].isString()) {
        cb(validationError("name required"));
        return;
    }
    if (!body->isMember("email") || !(*body)["email"].isString()) {
        cb(validationError("email required"));
        return;
    }
    if (!body->isMember("password") || !(*body)["password"].isString()) {
        cb(validationError("password required"));
        return;
    }
    auto name = (*body)["name"].asString();
    auto email = (*body)["email"].asString();
    auto password = (*body)["password"].asString();
    try {
        auto result = authService_->registerUser(name, email, password);
        cb(successResponse(result));
    } catch (const ServiceError& err) {
        cb(errorResponse(err.status(), err.what()));
    } catch (const drogon::orm::DrogonDbException& e) {
        cb(errorResponse(drogon::k500InternalServerError,
                         std::string("db error: ") + e.base().what()));
    }
}

void AuthController::handleLogin(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto body = req->getJsonObject();
    if (!body) {
        cb(validationError("invalid JSON payload"));
        return;
    }
    if (!body->isMember("email") || !(*body)["email"].isString()) {
        cb(validationError("email required"));
        return;
    }
    if (!body->isMember("password") || !(*body)["password"].isString()) {
        cb(validationError("password required"));
        return;
    }
    auto email = (*body)["email"].asString();
    auto password = (*body)["password"].asString();
    try {
        auto result = authService_->login(email, password);
        cb(successResponse(result));
    } catch (const ServiceError& err) {
        cb(errorResponse(err.status(), err.what()));
    } catch (const drogon::orm::DrogonDbException& e) {
        cb(errorResponse(drogon::k500InternalServerError,
                         std::string("db error: ") + e.base().what()));
    }
}

drogon::HttpResponsePtr AuthController::validationError(const std::string& message) const {
    return errorResponse(drogon::k400BadRequest, message);
}

drogon::HttpResponsePtr AuthController::successResponse(const AuthService::LoginResult& result) const {
    Json::Value json;
    json["user_id"] = static_cast<Json::Int64>(result.user.id);
    json["name"] = result.user.name;
    json["email"] = result.user.email;
    json["token"] = result.token;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(drogon::k200OK);
    return resp;
}

drogon::HttpResponsePtr AuthController::errorResponse(drogon::HttpStatusCode status,
                                                      const std::string& message) const {
    Json::Value json;
    json["error"] = message;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(status);
    return resp;
}
