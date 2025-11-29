#include "UrlShortenerService.h"
#include "controllers/AuthController.h"
#include "services/AuthService.h"
#include "services/DataStore.h"
#include "security/JwtService.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>

using namespace std;
using namespace drogon;

namespace {
struct AppSettings {
    std::string baseUrl;
    std::string dbUrl;
    size_t dbPoolSize{4};
    std::string jwtSecret;
    std::chrono::seconds jwtTtl{std::chrono::seconds{3600}};
};

std::optional<std::string> readString(const Json::Value& node, const char* field) {
    if (node.isMember(field) && node[field].isString()) {
        auto value = node[field].asString();
        if (!value.empty()) {
            return value;
        }
    }
    return std::nullopt;
}

AppSettings loadSettings(const Json::Value& config) {
    AppSettings settings;

    if (auto rootBase = readString(config, "base_url")) {
        settings.baseUrl = *rootBase;
    }

    if (config.isMember("app") && config["app"].isObject()) {
        if (auto nestedBase = readString(config["app"], "base_url")) {
            settings.baseUrl = *nestedBase;
        }
        if (auto nestedDb = readString(config["app"], "database_url")) {
            settings.dbUrl = *nestedDb;
        }
    }

    if (auto rootDb = readString(config, "database_url")) {
        settings.dbUrl = *rootDb;
    }

    if (config.isMember("database") && config["database"].isObject()) {
        const auto& db = config["database"];
        if (auto nestedDb = readString(db, "url")) {
            settings.dbUrl = *nestedDb;
        }
        if (auto poolSize = readString(db, "pool_size")) {
            try {
                settings.dbPoolSize = std::stoul(*poolSize);
            } catch (...) {
                throw std::runtime_error("database.pool_size must be numeric");
            }
        } else if (db.isMember("pool_size") && db["pool_size"].isUInt()) {
            settings.dbPoolSize = db["pool_size"].asUInt();
        }
    }

    if (config.isMember("security") && config["security"].isObject()) {
        const auto& security = config["security"];
        if (auto secret = readString(security, "jwt_secret")) {
            settings.jwtSecret = *secret;
        }
        if (auto ttl = readString(security, "jwt_ttl_seconds")) {
            try {
                settings.jwtTtl = std::chrono::seconds{std::stoll(*ttl)};
            } catch (...) {
                throw std::runtime_error("security.jwt_ttl_seconds must be numeric");
            }
        } else if (security.isMember("jwt_ttl_seconds") && security["jwt_ttl_seconds"].isInt64()) {
            settings.jwtTtl = std::chrono::seconds{security["jwt_ttl_seconds"].asInt64()};
        }
    }

    if (settings.baseUrl.empty()) {
        if (const char* envBase = std::getenv("BASE_URL")) {
            settings.baseUrl = envBase;
        }
    }

    if (settings.dbUrl.empty()) {
        if (const char* envDb = std::getenv("DATABASE_URL")) {
            settings.dbUrl = envDb;
        }
    }

    if (settings.jwtSecret.empty()) {
        if (const char* envSecret = std::getenv("JWT_SECRET")) {
            settings.jwtSecret = envSecret;
        }
    }

    if (const char* envTtl = std::getenv("JWT_TTL_SECONDS")) {
        try {
            settings.jwtTtl = std::chrono::seconds{std::stoll(envTtl)};
        } catch (...) {
            throw std::runtime_error("JWT_TTL_SECONDS must be numeric");
        }
    }

    if (settings.dbUrl.empty()) {
        throw std::runtime_error("DATABASE_URL or database.url config must be set");
    }

    if (settings.jwtSecret.empty()) {
        throw std::runtime_error("JWT_SECRET or security.jwt_secret config must be set");
    }

    if (settings.dbPoolSize == 0) {
        settings.dbPoolSize = 4;
    }

    return settings;
}
}

int main() {
    auto& app = drogon::app();
    app.loadConfigFile("config.json");

    auto settings = loadSettings(app.getCustomConfig());

    auto dataStore = make_shared<DataStore>(settings.dbUrl, settings.dbPoolSize);
    auto jwtService = make_shared<JwtService>(settings.jwtSecret, settings.jwtTtl);
    auto authService = make_shared<AuthService>(dataStore, jwtService);
    auto authController = make_shared<AuthController>(authService);
    auto urlService = make_shared<UrlShortenerService>(dataStore, authService, settings.baseUrl);
    
    app.registerHandler("/", [](const HttpRequestPtr&, function<void(const HttpResponsePtr&)>&& cb) {
        auto resp = HttpResponse::newFileResponse("public/index.html");
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_TEXT_HTML);
        cb(resp);
    }, {Get});

    app.registerHandler("/api/v1/health", 
        [urlService](const HttpRequestPtr& req, function<void(const HttpResponsePtr&)>&& callback) {
            urlService->handleHealth(req, move(callback));
        }, {Get});
        
    app.registerHandler("/api/v1/shorten",
        [urlService](const HttpRequestPtr& req, function<void(const HttpResponsePtr&)>&& callback) {
            urlService->handleShorten(req, move(callback));
        }, {Post});

    app.registerHandler("/api/v1/urls",
        [urlService](const HttpRequestPtr& req, function<void(const HttpResponsePtr&)>&& callback) {
            urlService->handleListUserUrls(req, move(callback));
        }, {Get});
        
    app.registerHandler("/api/v1/info/{1}",
        [urlService](const HttpRequestPtr& req, function<void(const HttpResponsePtr&)>&& callback, const string& code) {
            urlService->handleInfo(req, move(callback), code);
        }, {Get});
        
    app.registerHandler("/{1}",
        [urlService](const HttpRequestPtr& req, function<void(const HttpResponsePtr&)>&& callback, const string& code) {
            urlService->handleResolve(req, move(callback), code);
        }, {Get});

    app.registerHandler("/api/v1/register",
        [authController](const HttpRequestPtr& req, function<void(const HttpResponsePtr&)>&& callback) {
            authController->handleRegister(req, move(callback));
        }, {Post});

    app.registerHandler("/api/v1/login",
        [authController](const HttpRequestPtr& req, function<void(const HttpResponsePtr&)>&& callback) {
            authController->handleLogin(req, move(callback));
        }, {Post});
    
    app.run();
}
