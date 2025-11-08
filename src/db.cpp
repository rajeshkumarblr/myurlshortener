#include "db.h"
#include <libpq-fe.h>
#include <cstdlib>
#include <mutex>
#include <iostream>
#include <stdexcept>

namespace db {
namespace {
std::mutex g_mu;
PGconn* g_conn = nullptr;

std::string conninfo() {
    // Prefer single connection string envs if present
    if (const char* uri = std::getenv("DATABASE_URL")) {
        return std::string(uri); // postgres://user:pass@host:port/db?params
    }
    if (const char* uri2 = std::getenv("PGURI")) {
        return std::string(uri2);
    }
    // Fallback to discrete key=value style
    const char* host = std::getenv("PGHOST");
    const char* port = std::getenv("PGPORT");
    const char* user = std::getenv("PGUSER");
    const char* pass = std::getenv("PGPASSWORD");
    const char* db   = std::getenv("PGDATABASE");
    std::string ci;
    if (host) ci += std::string("host=") + host + " ";
    if (port) ci += std::string("port=") + port + " ";
    if (user) ci += std::string("user=") + user + " ";
    if (pass) ci += std::string("password=") + pass + " ";
    if (db)   ci += std::string("dbname=") + db + " ";
    return ci;
}

void ensure_connection_locked() {
    if (g_conn && PQstatus(g_conn) == CONNECTION_OK) return;
    if (g_conn) { PQfinish(g_conn); g_conn = nullptr; }
    g_conn = PQconnectdb(conninfo().c_str());
    if (PQstatus(g_conn) != CONNECTION_OK) {
        std::string err = PQerrorMessage(g_conn);
        PQfinish(g_conn);
        g_conn = nullptr;
        throw std::runtime_error("PostgreSQL connect failed: " + err);
    }
}

void exec_ddl_locked(const char* sql) {
    PGresult* res = PQexec(g_conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::string err = PQerrorMessage(g_conn);
        PQclear(res);
        throw std::runtime_error("PostgreSQL DDL failed: " + err);
    }
    PQclear(res);
}
}

void init() {
    std::lock_guard<std::mutex> lk(g_mu);
    auto ci = conninfo();
    if (ci.empty()) {
        throw std::runtime_error("No PostgreSQL connection information provided. Set DATABASE_URL or PG* environment variables.");
    }
    // Basic redaction (remove password= segment if using key=value form)
    std::string redacted = ci;
    auto pwPos = redacted.find("password=");
    if (pwPos != std::string::npos) {
        auto end = redacted.find(' ', pwPos);
        redacted.replace(pwPos, (end == std::string::npos ? redacted.size() : end) - pwPos, "password=***");
    }
    // If URI form, redact after ':' before '@'
    if (redacted.rfind("postgres://", 0) == 0 || redacted.rfind("postgresql://",0)==0) {
        auto schemeEnd = redacted.find("://");
        auto atPos = redacted.find('@', schemeEnd + 3);
        auto colonPos = redacted.find(':', schemeEnd + 3);
        if (colonPos != std::string::npos && atPos != std::string::npos && colonPos < atPos) {
            redacted.replace(colonPos + 1, atPos - (colonPos + 1), "***");
        }
    }
    std::cerr << "[db] Connecting using: " << redacted << std::endl;
    ensure_connection_locked();
    // Verify we connected to expected database
    {
        PGresult* res = PQexec(g_conn, "SELECT current_database();");
        if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) == 1) {
            // no-op; could log current_database if needed
        }
        PQclear(res);
    }
    const char* ddl = R"SQL(
CREATE TABLE IF NOT EXISTS url_mapping (
  code VARCHAR(16) PRIMARY KEY,
  url  TEXT NOT NULL,
  expires_at TIMESTAMPTZ NULL
);
CREATE INDEX IF NOT EXISTS idx_url_mapping_expires_at ON url_mapping(expires_at);
)SQL";
    exec_ddl_locked(ddl);
}

static std::optional<long long> to_epoch_ms(const std::optional<TimePoint>& t) {
    if (!t) return std::nullopt;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t->time_since_epoch()).count();
    return ms;
}

bool insert_mapping(const std::string& code, const std::string& url,
                    const std::optional<TimePoint>& expires_at) {
    std::lock_guard<std::mutex> lk(g_mu);
    ensure_connection_locked();

    std::string sql;
    if (expires_at) {
        // Use to_timestamp with milliseconds
        sql = "INSERT INTO url_mapping(code,url,expires_at) VALUES($1,$2, to_timestamp($3/1000.0)) ON CONFLICT DO NOTHING";
    } else {
        sql = "INSERT INTO url_mapping(code,url,expires_at) VALUES($1,$2, NULL) ON CONFLICT DO NOTHING";
    }

    const char* paramValues[3];
    int nParams = 0;
    paramValues[nParams++] = code.c_str();
    paramValues[nParams++] = url.c_str();
    std::string msStr;
    if (expires_at) {
        msStr = std::to_string(*to_epoch_ms(expires_at));
        paramValues[nParams++] = msStr.c_str();
    }

    PGresult* res = PQexecParams(g_conn, sql.c_str(), nParams, nullptr, paramValues, nullptr, nullptr, 0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::string err = PQerrorMessage(g_conn);
        PQclear(res);
        throw std::runtime_error("insert failed: " + err);
    }
    // Check rows affected
    char* aff = PQcmdTuples(res);
    bool inserted = (aff && std::string(aff) != "0");
    PQclear(res);
    return inserted;
}

static bool select_common(const std::string& code, std::string* url_out, bool* ttl_active) {
    const char* sql =
        "SELECT url, (expires_at IS NOT NULL) AS ttl_active "
        "FROM url_mapping WHERE code=$1 AND (expires_at IS NULL OR expires_at > NOW())";
    const char* paramValues[1] = { code.c_str() };

    PGresult* res = PQexecParams(g_conn, sql, 1, nullptr, paramValues, nullptr, nullptr, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::string err = PQerrorMessage(g_conn);
        PQclear(res);
        throw std::runtime_error("select failed: " + err);
    }
    int rows = PQntuples(res);
    if (rows < 1) { PQclear(res); return false; }
    if (url_out) *url_out = PQgetvalue(res, 0, 0);
    if (ttl_active) *ttl_active = std::string(PQgetvalue(res, 0, 1)) == "t";
    PQclear(res);
    return true;
}

bool get_mapping(const std::string& code, std::string& out_url) {
    std::lock_guard<std::mutex> lk(g_mu);
    ensure_connection_locked();
    return select_common(code, &out_url, nullptr);
}

bool get_info(const std::string& code, std::string& out_url, bool& ttl_active) {
    std::lock_guard<std::mutex> lk(g_mu);
    ensure_connection_locked();
    return select_common(code, &out_url, &ttl_active);
}

bool ping() {
    std::lock_guard<std::mutex> lk(g_mu);
    try {
        ensure_connection_locked();
        PGresult* res = PQexec(g_conn, "SELECT 1");
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::string err = PQerrorMessage(g_conn);
            PQclear(res);
            return false;
        }
        PQclear(res);
        return true;
    } catch(...) {
        return false;
    }
}
}
