#pragma once
#include "base_client.h"

namespace providers {

class OpenAIClient : public BaseClient {
public:
    Response sendRequest(const config::Target& target, const RequestContext& ctx) override;
};

}
