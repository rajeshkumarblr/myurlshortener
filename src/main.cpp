#include "UrlShortenerService.h"
#include <memory>

using namespace std;
using namespace drogon;

int main() {
    // Get app reference and load config before constructing services so they see custom settings
    auto& app = drogon::app();
    app.loadConfigFile("config.json");

    // Create service instance
    auto urlService = make_shared<UrlShortenerService>();
    
    app.registerHandler("/", [](const HttpRequestPtr&, function<void(const HttpResponsePtr&)>&& cb) {
        auto resp = HttpResponse::newFileResponse("public/index.html");
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_TEXT_HTML);
        cb(resp);
    }, {Get});

    // Register routes with proper C++ member function binding
    app.registerHandler("/api/v1/health", 
        [urlService](const HttpRequestPtr& req, function<void(const HttpResponsePtr&)>&& callback) {
            urlService->handleHealth(req, move(callback));
        }, {Get});
        
    app.registerHandler("/api/v1/shorten",
        [urlService](const HttpRequestPtr& req, function<void(const HttpResponsePtr&)>&& callback) {
            urlService->handleShorten(req, move(callback));
        }, {Post});
        
    app.registerHandler("/api/v1/info/{1}",
        [urlService](const HttpRequestPtr& req, function<void(const HttpResponsePtr&)>&& callback, const string& code) {
            urlService->handleInfo(req, move(callback), code);
        }, {Get});
        
    app.registerHandler("/{1}",
        [urlService](const HttpRequestPtr& req, function<void(const HttpResponsePtr&)>&& callback, const string& code) {
            urlService->handleResolve(req, move(callback), code);
        }, {Get});
    
    // Run the application
    app.run();
}
