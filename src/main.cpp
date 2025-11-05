#include <drogon/drogon.h>
#include <json/json.h>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <chrono>
#include <random>
#include <sstream>

using namespace std;
using namespace drogon;
using Response = HttpResponse;

using Clock = chrono::steady_clock;

struct UrlRec {
  string original;
  optional<Clock::time_point> expires_at; // nullopt => no TTL
};

static unordered_map<string, UrlRec> g_store;
static shared_mutex g_mu;

// ---- base62 helpers ----
static const char* B62 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

string base62(uint64_t x, size_t width = 7) {
  string s;
  while (x > 0) { s.push_back(B62[x % 62]); x /= 62; }
  while (s.size() < width) s.push_back('0');
  reverse(s.begin(), s.end());
  return s;
}

string gen_code() {
  static thread_local mt19937_64 rng{random_device{}()};
  uniform_int_distribution<uint64_t> dist;
  return base62(dist(rng), 7);
}

bool expired(const UrlRec& r) {
  if (!r.expires_at) return false;
  return Clock::now() > *r.expires_at;
}

// ---- handlers ----
void handle_health(const HttpRequestPtr&,
                   function<void (const HttpResponsePtr &)> &&cb) {
  auto resp = Response::newHttpResponse();
  resp->setStatusCode(k200OK);
  resp->setContentTypeCode(CT_TEXT_PLAIN);
  resp->setBody("ok");
  cb(resp);
}

void handle_shorten(const HttpRequestPtr &req,
                    function<void (const HttpResponsePtr &)> &&cb) {
  auto body = req->getJsonObject();
  if (!body || !body->isMember("url") || !(*body)["url"].isString()) {
    auto resp = Response::newHttpJsonResponse(Json::Value{{"error","url required"}});
    resp->setStatusCode(k400BadRequest);
    cb(resp);
    return;
  }
  const string url = (*body)["url"].asString();
  int ttl = 0;
  if ((*body).isMember("ttl") && (*body)["ttl"].isInt()) ttl = (*body)["ttl"].asInt();

  // generate unique code (retry on collision)
  string code;
  UrlRec rec{url, ttl>0 ? optional{Clock::now()+chrono::seconds(ttl)} : nullopt};
  for (int i=0;i<5;i++) {
    code = gen_code();
    unique_lock lk(g_mu);
    if (!g_store.count(code)) { g_store[code] = rec; break; }
    if (i==4) {
      auto resp = Response::newHttpJsonResponse(Json::Value{{"error","collision"}});
      resp->setStatusCode(k500InternalServerError);
      cb(resp);
      return;
    }
  }

  Json::Value out;
  out["code"] = code;

  // pick port from environment variable (default 9090)
  const char* port_env = getenv("APP_PORT");
  string port = port_env ? port_env : "9090";
  out["short"] = "http://localhost:" + port + "/" + code;

  cb(Response::newHttpJsonResponse(out));
}

void handle_info(const HttpRequestPtr &,
                 function<void (const HttpResponsePtr &)> &&cb,
                 const string &code) {
  shared_lock lk(g_mu);
  auto it = g_store.find(code);
  Json::Value out;
  if (it == g_store.end() || expired(it->second)) {
    auto resp = Response::newHttpJsonResponse(Json::Value{{"error","not found"}});
    resp->setStatusCode(k404NotFound);
    cb(resp);
    return;
  }
  out["code"] = code;
  out["url"] = it->second.original;
  out["ttl_active"] = it->second.expires_at.has_value();
  cb(Response::newHttpJsonResponse(out));
}

void handle_resolve(const HttpRequestPtr &,
                    function<void (const HttpResponsePtr &)> &&cb,
                    const string &code) {
  shared_lock lk(g_mu);
  auto it = g_store.find(code);
  if (it == g_store.end() || expired(it->second)) {
    auto resp = Response::newHttpResponse();
    resp->setStatusCode(k404NotFound);
    cb(resp);
    return;
  }
  auto resp = Response::newRedirectionResponse(it->second.original);
  cb(resp);
}

int main() {
  // Get app reference once
  auto& appInstance = app();

  // Load configuration first
  appInstance.loadConfigFile("config.json");

  // Routes
  appInstance.registerHandler("/api/v1/health", &handle_health, {Get});
  appInstance.registerHandler("/api/v1/shorten", &handle_shorten, {Post});
  appInstance.registerHandler("/api/v1/info/{1}", &handle_info, {Get});
  appInstance.registerHandler("/{1}", &handle_resolve, {Get});

  // Run the application (config file settings will be used)
  appInstance.run();
}
