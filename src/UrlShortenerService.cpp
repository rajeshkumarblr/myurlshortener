#include "UrlShortenerService.h"
#include "utils.h"
#include <random>
#include <memory>
#include "db.h"

using namespace std;
using namespace drogon;
using Clock = chrono::steady_clock;

bool UrlRecord::isExpired() const {
    return expiresAt && Clock::now() > *expiresAt;
}

UrlShortenerService::UrlShortenerService() {
    loadConfiguration();
    try {
        db::init();
    } catch (const std::exception& e) {
        // DB init errors surface on first use; continue startup
    }
}

void UrlShortenerService::loadConfiguration() {
    try {
        auto& appConfig = drogon::app().getCustomConfig();
        const Json::Value* baseNode = nullptr;
        if (appConfig.isMember("base_url") && appConfig["base_url"].isString()) {
            baseNode = &appConfig["base_url"];
        } else if (appConfig.isMember("app") && appConfig["app"].isObject()) {
            const auto& appSection = appConfig["app"];
            if (appSection.isMember("base_url") && appSection["base_url"].isString()) {
                baseNode = &appSection["base_url"];
            }
        }

        if (baseNode != nullptr) {
            baseUrl = baseNode->asString();
        }
    } catch (const exception& e) {
        // If config loading fails, use fallback in getBaseUrl()
        baseUrl = "";
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
    if (!baseUrl.empty()) {
        return baseUrl;
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

size_t UrlShortenerService::getShardIndex(const string& key) const {
    // Simple hash-based sharding
    return hash<string>{}(key) % NUM_SHARDS;
}

void UrlShortenerService::handleHealth(const HttpRequestPtr& req, 
                                      function<void(const HttpResponsePtr&)>&& callback) const {
    bool db_ok = db::ping();
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
    
    // Parse TTL if provided
    int ttlSeconds = 0;
    if (body->isMember("ttl") && (*body)["ttl"].isInt()) {
        ttlSeconds = (*body)["ttl"].asInt();
    }
    
    // Generate unique code with DB-backed collision retry
    string code;
    auto expiresAt = ttlSeconds > 0 ? optional{chrono::time_point<chrono::system_clock>(chrono::system_clock::now() + chrono::seconds(ttlSeconds))} : nullopt;
    
    // Generate code and use shard-specific mutex
    for (int attempt = 0; attempt < 5; ++attempt) {
        code = generateShortCode();
        try {
            if (db::insert_mapping(code, url, expiresAt)) {
                break; // success
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
    string url;
    bool ttl_active = false;
    try {
        if (!db::get_info(code, url, ttl_active)) {
            callback(createErrorResponse("not found", k404NotFound));
            return;
        }
    } catch (const std::exception& e) {
        callback(createErrorResponse(string("db error: ") + e.what(), k500InternalServerError));
        callback(createErrorResponse("not found", k404NotFound));
        return;
    }
    
    Json::Value response;
    response["code"] = code;
    response["url"] = url;
    response["ttl_active"] = ttl_active;
    
    callback(createJsonResponse(response));
}

void UrlShortenerService::handleResolve(const HttpRequestPtr& req, 
                                       function<void(const HttpResponsePtr&)>&& callback, 
                                       const string& code) const {
    std::string url;
    try {
        if (!db::get_mapping(code, url)) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k404NotFound);
            callback(resp);
            return;
        }
    } catch (const std::exception& e) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
        return;
    }
    callback(HttpResponse::newRedirectionResponse(url));
}