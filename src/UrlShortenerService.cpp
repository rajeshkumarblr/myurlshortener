#include "UrlShortenerService.h"
#include "utils.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <random>

using namespace std;
using namespace drogon;
using SystemClock = std::chrono::system_clock;

UrlShortenerService::UrlShortenerService(std::shared_ptr<DataStore> dataStore,
                                         std::shared_ptr<AuthService> authService,
                                         std::string baseUrl)
    : dataStore_(std::move(dataStore)),
      authService_(std::move(authService)),
      baseUrl_(std::move(baseUrl)) {
    if (!dataStore_ || !authService_) {
        throw std::runtime_error("Service dependencies missing");
    }
}

HttpResponsePtr UrlShortenerService::createJsonResponse(const Json::Value& data, HttpStatusCode status) const {
    auto resp = HttpResponse::newHttpJsonResponse(data);
    resp->setStatusCode(status);
    return resp;
}

HttpResponsePtr UrlShortenerService::createErrorResponse(const string& message, HttpStatusCode status) const {
    Json::Value json;
    json["error"] = message;
    return createJsonResponse(json, status);
}

string UrlShortenerService::generateShortCode() const {
    static thread_local mt19937_64 rng{random_device{}()};
    uniform_int_distribution<uint64_t> dist;
    
    // Use optimized fast path for width=7 (most common case)
    return Base62::encodeFast7(dist(rng));
}

string UrlShortenerService::getBaseUrl() const {
    if (!baseUrl_.empty()) {
        return baseUrl_;
    }
    
    // Fallback to environment variable or default
    if (const char* envBase = getenv("BASE_URL")) {
        if (*envBase) {
            return string(envBase);
        }
    }
    const char* portEnv = getenv("APP_PORT");
    string port = portEnv ? portEnv : "9090";
    return "http://localhost:" + port;
}

void UrlShortenerService::handleHealth(const HttpRequestPtr& req, 
                                      function<void(const HttpResponsePtr&)>&& callback) const {
    bool db_ok = dataStore_->ping();
    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeCode(CT_TEXT_PLAIN);
    if (db_ok) {
        resp->setStatusCode(k200OK);
        resp->setBody("ok");
    } else {
        resp->setStatusCode(k503ServiceUnavailable);
        resp->setBody("db-unavailable");
    }
    callback(resp);
}

void UrlShortenerService::handleShorten(const HttpRequestPtr& req, 
                                       function<void(const HttpResponsePtr&)>&& callback) {
    auto body = req->getJsonObject();
    
    // Validate request
    if (!body || !body->isMember("url") || !(*body)["url"].isString()) {
        callback(createErrorResponse("url required"));
        return;
    }
    
    const string url = (*body)["url"].asString();
    if (url.empty()) {
        callback(createErrorResponse("url cannot be empty"));
        return;
    }
    
    auto user = authService_->authenticate(req);
    if (!user) {
        callback(createErrorResponse("authentication required", k401Unauthorized));
        return;
    }

    int ttlSeconds = 0;
    if (body->isMember("ttl") && (*body)["ttl"].isInt()) {
        ttlSeconds = (*body)["ttl"].asInt();
    }

    std::optional<DataStore::TimePoint> expiresAt;
    if (ttlSeconds > 0) {
        expiresAt = SystemClock::now() + std::chrono::seconds(ttlSeconds);
    }

    string code;
    for (int attempt = 0; attempt < 5; ++attempt) {
        code = generateShortCode();
        try {
            if (dataStore_->insertMapping(code, url, expiresAt, user->id)) {
                break;
            }
        } catch (const std::exception& e) {
            callback(createErrorResponse(string("db error: ") + e.what(), k500InternalServerError));
            return;
        }

        if (attempt == 4) {
            callback(createErrorResponse("collision", k500InternalServerError));
            return;
        }
    }
    
    // Build response
    Json::Value response;
    response["code"] = code;
    response["short"] = getBaseUrl() + "/" + code;
    
    callback(createJsonResponse(response));
}

void UrlShortenerService::handleInfo(const HttpRequestPtr& req, 
                                    function<void(const HttpResponsePtr&)>&& callback, 
                                    const string& code) const {
    try {
        auto info = dataStore_->getUrlInfo(code);
        if (!info) {
            callback(createErrorResponse("not found", k404NotFound));
            return;
        }
        Json::Value response;
        response["code"] = code;
        response["url"] = info->url;
        response["ttl_active"] = info->ttlActive;
        callback(createJsonResponse(response));
    } catch (const std::exception& e) {
        callback(createErrorResponse(string("db error: ") + e.what(), k500InternalServerError));
        return;
    }
}

void UrlShortenerService::handleResolve(const HttpRequestPtr& req, 
                                       function<void(const HttpResponsePtr&)>&& callback, 
                                       const string& code) const {
    try {
        auto url = dataStore_->resolveUrl(code);
        if (!url) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k404NotFound);
            callback(resp);
            return;
        }
        callback(HttpResponse::newRedirectionResponse(*url));
    } catch (const std::exception& e) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
        return;
    }
}

void UrlShortenerService::handleListUserUrls(
    const HttpRequestPtr& req,
    function<void(const HttpResponsePtr&)>&& callback) const {
    auto user = authService_->authenticate(req);
    if (!user) {
        callback(createErrorResponse("authentication required", k401Unauthorized));
        return;
    }
    size_t limit = 50;
    auto limitParam = req->getParameter("limit");
    if (!limitParam.empty()) {
        try {
            limit = std::clamp(static_cast<size_t>(std::stoul(limitParam)), size_t{1}, size_t{200});
        } catch (...) {
            callback(createErrorResponse("invalid limit value"));
            return;
        }
    }

    try {
        auto rows = dataStore_->listUrlsForUser(user->id, limit);
        Json::Value json(Json::arrayValue);
        for (const auto& row : rows) {
            Json::Value item;
            item["code"] = row.code;
            item["url"] = row.url;
            item["short"] = getBaseUrl() + "/" + row.code;
            item["created_at"] = static_cast<Json::Int64>(std::chrono::duration_cast<std::chrono::seconds>(row.createdAt.time_since_epoch()).count());
            if (row.expiresAt) {
                item["expires_at"] = static_cast<Json::Int64>(std::chrono::duration_cast<std::chrono::seconds>(row.expiresAt->time_since_epoch()).count());
            }
            json.append(item);
        }
        callback(createJsonResponse(json));
    } catch (const std::exception& e) {
        callback(createErrorResponse(string("db error: ") + e.what(), k500InternalServerError));
    }
}