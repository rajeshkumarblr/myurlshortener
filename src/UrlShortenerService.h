#pragma once
#include "services/AuthService.h"
#include "services/DataStore.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <drogon/nosql/RedisClient.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>

class UrlShortenerService {
private:
    std::shared_ptr<DataStore> dataStore_;
    std::shared_ptr<AuthService> authService_;
    std::string baseUrl_;
    drogon::nosql::RedisClientPtr redisClient_;

    drogon::HttpResponsePtr createJsonResponse(
        const Json::Value& data,
        drogon::HttpStatusCode status = drogon::k200OK) const;

    drogon::HttpResponsePtr createErrorResponse(
        const std::string& message,
        drogon::HttpStatusCode status = drogon::k400BadRequest) const;

    std::string generateShortCode() const;
    std::string getBaseUrl() const;

public:
    UrlShortenerService(std::shared_ptr<DataStore> dataStore,
                        std::shared_ptr<AuthService> authService,
                        std::string baseUrl,
                        drogon::nosql::RedisClientPtr redisClient);
    
    // Health check endpoint
    void handleHealth(const drogon::HttpRequestPtr& req, 
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
    
    // Shorten URL endpoint
    void handleShorten(const drogon::HttpRequestPtr& req, 
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    
    // Get URL info endpoint
    void handleInfo(const drogon::HttpRequestPtr& req, 
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback, 
                   const std::string& code) const;
    
    // Resolve and redirect endpoint
    void handleResolve(const drogon::HttpRequestPtr& req, 
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback, 
                      const std::string& code) const;
    
    void handleListUserUrls(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;
};