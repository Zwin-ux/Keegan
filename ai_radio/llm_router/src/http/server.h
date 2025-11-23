#pragma once
#include "httplib.h"
#include "routing/router.h"

namespace http {

class Server {
public:
    Server(routing::Router& router, int port);
    void start();

private:
    routing::Router& router_;
    int port_;
    httplib::Server svr_;
};

}
