#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <chrono>
#include <array>
#include <functional>

// URL record with TTL support
struct UrlRecord {
    std::string originalUrl;
    std::optional<std::chrono::steady_clock::time_point> expiresAt;
    
    bool isExpired() const;
};

// Main URL Shortener Service Class
class UrlShortenerService {
private:
    std::unordered_map<std::string, UrlRecord> urlStore;
    
    // Sharded mutexes for better concurrency
    static constexpr size_t NUM_SHARDS = 16;
    mutable std::array<std::shared_mutex, NUM_SHARDS> shardMutexes;
    
    std::string baseUrl;
    
    // Helper methods
    drogon::HttpResponsePtr createJsonResponse(const Json::Value& data, 
                                             drogon::HttpStatusCode status = drogon::k200OK) const;
    
    drogon::HttpResponsePtr createErrorResponse(const std::string& message, 
                                              drogon::HttpStatusCode status = drogon::k400BadRequest) const;
    
    std::string generateShortCode() const;
    std::string getBaseUrl() const;
    
    // Get shard index for a given key
    size_t getShardIndex(const std::string& key) const;

public:
    // Constructor - initialize with configuration
    UrlShortenerService();
    
    // Load configuration from Drogon app settings
    void loadConfiguration();
    
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
};