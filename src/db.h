#pragma once
#include <string>
#include <optional>
#include <chrono>

namespace db {
using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

// Optionally override connection URI before init(); clears on empty string.
void set_connection_uri(const std::string& uri);

// Initialize connection pool and ensure schema exists.
void init();

// Insert mapping; returns true if inserted, false on conflict.
bool insert_mapping(const std::string& code, const std::string& url,
                    const std::optional<TimePoint>& expires_at);

// Fetch mapping; returns true and fills url if found and not expired.
bool get_mapping(const std::string& code, std::string& out_url);

// Fetch info; returns tuple(url, ttl_active). Returns false if not found/expired.
bool get_info(const std::string& code, std::string& out_url, bool& ttl_active);

// Lightweight connectivity check (no schema validation besides existing table creation on init).
bool ping();
}
