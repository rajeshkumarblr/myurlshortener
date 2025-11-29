#include "DataStore.h"
#include <drogon/orm/Exception.h>
#include <trantor/utils/Date.h>
#include <stdexcept>

using namespace std::chrono;

namespace {
constexpr size_t kDefaultPoolSize = 4;
}

DataStore::DataStore(const std::string& uri, size_t poolSize) {
    if (uri.empty()) {
        throw std::runtime_error("Database URI is required");
    }
    const auto pool = poolSize == 0 ? kDefaultPoolSize : poolSize;
    client_ = drogon::orm::DbClient::newPgClient(uri, pool);
    if (!client_) {
        throw std::runtime_error("Failed to create Drogon DbClient");
    }
}

bool DataStore::ping() const {
    try {
        auto r = client_->execSqlSync("SELECT 1");
        return !r.empty();
    } catch (...) {
        return false;
    }
}

bool DataStore::insertMapping(const std::string& code,
                              const std::string& url,
                              const std::optional<TimePoint>& expiresAt,
                              const std::optional<long>& userId) {
    std::optional<trantor::Date> expiresArg;
    if (expiresAt) {
        auto micros = duration_cast<microseconds>(expiresAt->time_since_epoch()).count();
        expiresArg = trantor::Date(micros);
    }
    auto result = client_->execSqlSync(
        "INSERT INTO url_mapping(code,url,expires_at,user_id) VALUES($1,$2,$3,$4) ON CONFLICT DO NOTHING",
        code,
        url,
        expiresArg,
        userId);
    return result.affectedRows() > 0;
}

std::optional<std::string> DataStore::resolveUrl(const std::string& code) const {
    auto res = client_->execSqlSync(
        "SELECT url FROM url_mapping WHERE code=$1 AND (expires_at IS NULL OR expires_at > NOW())",
        code);
    if (res.empty()) {
        return std::nullopt;
    }
    return res[0]["url"].as<std::string>();
}

std::optional<DataStore::UrlInfo> DataStore::getUrlInfo(const std::string& code) const {
    auto res = client_->execSqlSync(
        "SELECT url, (expires_at IS NOT NULL) as ttl_active FROM url_mapping WHERE code=$1 AND (expires_at IS NULL OR expires_at > NOW())",
        code);
    if (res.empty()) {
        return std::nullopt;
    }
    UrlInfo info;
    info.url = res[0]["url"].as<std::string>();
    info.ttlActive = res[0]["ttl_active"].as<bool>();
    return info;
}

std::optional<DataStore::UserRecord> DataStore::findUserByEmail(const std::string& email) const {
    auto res = client_->execSqlSync(
        "SELECT id,name,email,password_hash FROM app_user WHERE email=$1",
        email);
    if (res.empty()) {
        return std::nullopt;
    }
    UserRecord user;
    user.id = res[0]["id"].as<long>();
    user.name = res[0]["name"].as<std::string>();
    user.email = res[0]["email"].as<std::string>();
    user.passwordHash = res[0]["password_hash"].as<std::string>();
    return user;
}

long DataStore::createUser(const std::string& name,
                           const std::string& email,
                           const std::string& passwordHash) {
    auto res = client_->execSqlSync(
        "INSERT INTO app_user(name,email,password_hash) VALUES($1,$2,$3) RETURNING id",
        name,
        email,
        passwordHash);
    return res[0]["id"].as<long>();
}

std::vector<DataStore::UrlListItem> DataStore::listUrlsForUser(long userId,
                                                               size_t limit) const {
    auto res = client_->execSqlSync(
        R"SQL(
        SELECT code,
               url,
               EXTRACT(EPOCH FROM created_at)::bigint AS created_epoch,
               EXTRACT(EPOCH FROM expires_at)::bigint AS expires_epoch
          FROM url_mapping
         WHERE user_id=$1
         ORDER BY created_at DESC
         LIMIT $2
        )SQL",
        userId,
        static_cast<long>(limit));
    std::vector<UrlListItem> items;
    items.reserve(res.size());
    for (const auto& row : res) {
        UrlListItem item;
        item.code = row["code"].as<std::string>();
        item.url = row["url"].as<std::string>();
        auto createdEpoch = row["created_epoch"].as<long long>();
        item.createdAt = TimePoint(seconds(createdEpoch));
        if (!row["expires_epoch"].isNull()) {
            auto expiresEpoch = row["expires_epoch"].as<long long>();
            item.expiresAt = TimePoint(seconds(expiresEpoch));
        }
        items.push_back(std::move(item));
    }
    return items;
}
