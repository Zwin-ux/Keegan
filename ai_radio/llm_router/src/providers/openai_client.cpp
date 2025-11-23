#include "openai_client.h"
#include "logging/logger.h"
#include <cstdlib>
#include <array>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <fstream>

namespace providers {

std::pair<int, std::string> exec(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return {0, result};
}

Response OpenAIClient::sendRequest(const config::Target& target, const RequestContext& ctx) {
    std::string tempFile = "temp_req_" + std::to_string(rand()) + ".json";
    {
        std::ofstream f(tempFile);
        f << ctx.body;
    }
    
    const char* apiKey = std::getenv(target.apiKeyEnv.c_str());
    std::string authHeader = "Authorization: Bearer ";
    if (apiKey) {
        authHeader += apiKey;
    } else {
        logging::logError("API Key env var not found: " + target.apiKeyEnv);
    }
    
    std::string url = target.baseUrl;
    if (url.back() == '/') url.pop_back();
    
    std::string path = ctx.path;
    if (url.ends_with("/v1") && path.starts_with("/v1")) {
        path = path.substr(3);
    }
    
    if (path.front() != '/') url += "/";
    url += path;
    
    std::string cmd = "curl.exe -s -w \"%{http_code}\" -X POST \"" + url + "\"";
    cmd += " -H \"Content-Type: application/json\"";
    cmd += " -H \"" + authHeader + "\"";
    cmd += " -d @" + tempFile;
    
    logging::logDebug("Executing curl: " + cmd);

    auto [rc, output] = exec(cmd);
    
    std::remove(tempFile.c_str());
    
    Response response;
    if (output.length() >= 3) {
        std::string codeStr = output.substr(output.length() - 3);
        try {
            response.status = std::stoi(codeStr);
            response.body = output.substr(0, output.length() - 3);
        } catch (...) {
            response.status = 500;
            response.body = output;
        }
    } else {
        response.status = 500;
        response.body = output;
    }
    response.contentType = "application/json";
    
    return response;
}

}
