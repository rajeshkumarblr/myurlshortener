#include "UrlShortenerService.h"
#include <memory>

using namespace std;
using namespace drogon;

int main() {
    // Create service instance
    auto urlService = make_shared<UrlShortenerService>();
    
    // Get app reference
    auto& app = drogon::app();
    
    // Load configuration
    app.loadConfigFile("config.json");
    
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
