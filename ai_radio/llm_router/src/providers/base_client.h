#pragma once
#include <string>
#include "config/loader.h"

namespace providers {

struct RequestContext {
    std::string path;
    std::string body;
    std::string method;
};

struct Response {
    int status;
    std::string body;
    std::string contentType;
};

class BaseClient {
public:
    virtual ~BaseClient() = default;
    virtual Response sendRequest(const config::Target& target, const RequestContext& ctx) = 0;
};

}
