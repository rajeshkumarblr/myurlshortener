#pragma once
#include <drogon/orm/DbClient.h>
#include <optional>
#include <string>
#include <vector>
#include <chrono>

class DataStore {
public:
    using SystemClock = std::chrono::system_clock;
    using TimePoint = SystemClock::time_point;

    struct UrlInfo {
        std::string url;
        bool ttlActive{false};
    };

    struct UrlListItem {
        std::string code;
        std::string url;
        std::optional<TimePoint> expiresAt;
        TimePoint createdAt;
    };

    struct UserRecord {
        long id{0};
        std::string name;
        std::string email;
        std::string passwordHash;
    };

    explicit DataStore(const std::string& uri, size_t poolSize = 4);

    bool ping() const;

    bool insertMapping(const std::string& code,
                       const std::string& url,
                       const std::optional<TimePoint>& expiresAt,
                       const std::optional<long>& userId);

    std::optional<std::string> resolveUrl(const std::string& code) const;
    std::optional<UrlInfo> getUrlInfo(const std::string& code) const;

    std::optional<UserRecord> findUserByEmail(const std::string& email) const;
    long createUser(const std::string& name,
                    const std::string& email,
                    const std::string& passwordHash);

    std::vector<UrlListItem> listUrlsForUser(long userId,
                                             size_t limit) const;

private:
    drogon::orm::DbClientPtr client_;
};
