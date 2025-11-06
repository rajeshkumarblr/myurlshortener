#include "UrlShortenerService.h"
#include "utils.h"
#include <random>
#include <memory>

using namespace std;
using namespace drogon;
using Clock = chrono::steady_clock;

bool UrlRecord::isExpired() const {
    return expiresAt && Clock::now() > *expiresAt;
}

UrlShortenerService::UrlShortenerService() {
    loadConfiguration();
}

void UrlShortenerService::loadConfiguration() {
    try {
        auto& appConfig = drogon::app().getCustomConfig();
        if (appConfig.isMember("base_url") && appConfig["base_url"].isString()) {
            baseUrl = appConfig["base_url"].asString();
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
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_TEXT_PLAIN);
    resp->setBody("ok");
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
    
    // Generate unique code with collision retry
    string code;
    UrlRecord record{
        url, 
        ttlSeconds > 0 ? optional{Clock::now() + chrono::seconds(ttlSeconds)} : nullopt
    };
    
    // Generate code and use shard-specific mutex
    for (int attempt = 0; attempt < 5; ++attempt) {
        code = generateShortCode();
        auto shardIndex = getShardIndex(code);
        
        unique_lock lock(shardMutexes[shardIndex]);
        if (urlStore.find(code) == urlStore.end()) {
            urlStore[code] = record;
            break;
        }
        lock.unlock(); // Release before retry
        
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
    auto shardIndex = getShardIndex(code);
    shared_lock lock(shardMutexes[shardIndex]);
    auto it = urlStore.find(code);
    
    if (it == urlStore.end() || it->second.isExpired()) {
        callback(createErrorResponse("not found", k404NotFound));
        return;
    }
    
    Json::Value response;
    response["code"] = code;
    response["url"] = it->second.originalUrl;
    response["ttl_active"] = it->second.expiresAt.has_value();
    
    callback(createJsonResponse(response));
}

void UrlShortenerService::handleResolve(const HttpRequestPtr& req, 
                                       function<void(const HttpResponsePtr&)>&& callback, 
                                       const string& code) const {
    auto shardIndex = getShardIndex(code);
    shared_lock lock(shardMutexes[shardIndex]);
    auto it = urlStore.find(code);
    
    if (it == urlStore.end() || it->second.isExpired()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k404NotFound);
        callback(resp);
        return;
    }
    
    callback(HttpResponse::newRedirectionResponse(it->second.originalUrl));
}