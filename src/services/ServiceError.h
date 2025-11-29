#pragma once
#include <drogon/HttpResponse.h>
#include <stdexcept>

class ServiceError : public std::runtime_error {
public:
    ServiceError(drogon::HttpStatusCode status, std::string message)
        : std::runtime_error(std::move(message)), status_(status) {}

    drogon::HttpStatusCode status() const noexcept {
        return status_;
    }

private:
    drogon::HttpStatusCode status_;
};
