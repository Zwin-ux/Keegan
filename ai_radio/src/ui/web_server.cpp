#include "web_server.h"
#include "../../vendor/httplib.h"
#include "../../vendor/vjson/vjson.h"
#include "../util/logger.h"
#include "../util/telemetry.h"
#include <filesystem>
#include <sstream>
#include <fstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <cstdlib>
#include <random>
#include <array>
#include <vector>
#include <cstring>
#include <algorithm>

namespace uisrv {

namespace {
// Helper to create JSON response
std::string makeJson(const std::string& key, const std::string& value) {
    return "{\"" + key + "\": \"" + value + "\"}";
}

std::string escapeJson(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string getEnvVar(const char* key) {
    const char* value = std::getenv(key);
    return value ? std::string(value) : std::string();
}

uint64_t nowMs() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

std::string randomHex(size_t bytes) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 255);
    std::stringstream ss;
    for (size_t i = 0; i < bytes; ++i) {
        int v = dist(rng);
        ss << std::hex << std::nouppercase;
        if (v < 16) ss << '0';
        ss << v;
    }
    return ss.str();
}

struct Sha1 {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    uint64_t lengthBits = 0;
    std::array<unsigned char, 64> buffer{};
    size_t bufferLen = 0;

    void processBlock(const unsigned char* block) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (block[i * 4] << 24) | (block[i * 4 + 1] << 16) |
                   (block[i * 4 + 2] << 8) | (block[i * 4 + 3]);
        }
        for (int i = 16; i < 80; ++i) {
            uint32_t v = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (v << 1) | (v >> 31);
        }

        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;
        uint32_t e = h4;

        for (int i = 0; i < 80; ++i) {
            uint32_t f = 0;
            uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    void update(const unsigned char* data, size_t len) {
        lengthBits += static_cast<uint64_t>(len) * 8;
        size_t offset = 0;

        if (bufferLen > 0) {
            size_t toCopy = std::min(len, 64 - bufferLen);
            std::memcpy(buffer.data() + bufferLen, data, toCopy);
            bufferLen += toCopy;
            offset += toCopy;
            if (bufferLen == 64) {
                processBlock(buffer.data());
                bufferLen = 0;
            }
        }

        while (offset + 64 <= len) {
            processBlock(data + offset);
            offset += 64;
        }

        if (offset < len) {
            bufferLen = len - offset;
            std::memcpy(buffer.data(), data + offset, bufferLen);
        }
    }

    std::array<unsigned char, 20> final() {
        buffer[bufferLen++] = 0x80;
        if (bufferLen > 56) {
            while (bufferLen < 64) buffer[bufferLen++] = 0x00;
            processBlock(buffer.data());
            bufferLen = 0;
        }
        while (bufferLen < 56) buffer[bufferLen++] = 0x00;

        uint64_t lenBits = lengthBits;
        for (int i = 7; i >= 0; --i) {
            buffer[bufferLen++] = static_cast<unsigned char>((lenBits >> (i * 8)) & 0xFF);
        }
        processBlock(buffer.data());
        bufferLen = 0;

        std::array<unsigned char, 20> out{};
        auto write32 = [&](int index, uint32_t value) {
            out[index] = static_cast<unsigned char>((value >> 24) & 0xFF);
            out[index + 1] = static_cast<unsigned char>((value >> 16) & 0xFF);
            out[index + 2] = static_cast<unsigned char>((value >> 8) & 0xFF);
            out[index + 3] = static_cast<unsigned char>(value & 0xFF);
        };

        write32(0, h0);
        write32(4, h1);
        write32(8, h2);
        write32(12, h3);
        write32(16, h4);
        return out;
    }
};

std::string sha1Hex(const std::string& input) {
    Sha1 sha;
    sha.update(reinterpret_cast<const unsigned char*>(input.data()), input.size());
    auto digest = sha.final();
    std::stringstream ss;
    for (unsigned char byte : digest) {
        ss << std::hex << std::nouppercase;
        if (byte < 16) ss << '0';
        ss << static_cast<int>(byte);
    }
    return ss.str();
}

std::string hmacSha1Hex(const std::string& key, const std::string& message) {
    std::array<unsigned char, 64> keyBlock{};
    if (key.size() > keyBlock.size()) {
        auto hashed = sha1Hex(key);
        for (size_t i = 0; i < keyBlock.size() && i * 2 + 1 < hashed.size(); ++i) {
            std::string byteStr = hashed.substr(i * 2, 2);
            keyBlock[i] = static_cast<unsigned char>(std::strtoul(byteStr.c_str(), nullptr, 16));
        }
    } else {
        std::memcpy(keyBlock.data(), key.data(), key.size());
    }

    std::array<unsigned char, 64> oKeyPad{};
    std::array<unsigned char, 64> iKeyPad{};
    for (size_t i = 0; i < 64; ++i) {
        oKeyPad[i] = keyBlock[i] ^ 0x5c;
        iKeyPad[i] = keyBlock[i] ^ 0x36;
    }

    Sha1 inner;
    inner.update(iKeyPad.data(), iKeyPad.size());
    inner.update(reinterpret_cast<const unsigned char*>(message.data()), message.size());
    auto innerDigest = inner.final();

    Sha1 outer;
    outer.update(oKeyPad.data(), oKeyPad.size());
    outer.update(innerDigest.data(), innerDigest.size());
    auto digest = outer.final();

    std::stringstream ss;
    for (unsigned char byte : digest) {
        ss << std::hex << std::nouppercase;
        if (byte < 16) ss << '0';
        ss << static_cast<int>(byte);
    }
    return ss.str();
}

struct TokenPayload {
    std::string stationId;
    uint64_t expiresAt = 0;
    std::string nonce;
};

std::string issueToken(const std::string& stationId, uint64_t expiresAt, const std::string& secret) {
    std::string nonce = randomHex(6);
    std::string message = "v1|" + stationId + "|" + std::to_string(expiresAt) + "|" + nonce;
    std::string sig = hmacSha1Hex(secret, message);
    return "v1." + stationId + "." + std::to_string(expiresAt) + "." + nonce + "." + sig;
}

bool parseToken(const std::string& token, TokenPayload& out, const std::string& secret) {
    std::vector<std::string> parts;
    size_t start = 0;
    size_t pos = token.find('.');
    while (pos != std::string::npos) {
        parts.push_back(token.substr(start, pos - start));
        start = pos + 1;
        pos = token.find('.', start);
    }
    parts.push_back(token.substr(start));
    if (parts.size() != 5) return false;
    if (parts[0] != "v1") return false;
    out.stationId = parts[1];
    try {
        out.expiresAt = std::stoull(parts[2]);
    } catch (...) {
        return false;
    }
    out.nonce = parts[3];
    std::string message = "v1|" + out.stationId + "|" + parts[2] + "|" + out.nonce;
    std::string expected = hmacSha1Hex(secret, message);
    return expected == parts[4];
}

std::string readTextFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.good()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string readCachedStationId() {
    auto data = readTextFile("cache/station_id.txt");
    if (data.empty()) return "";
    // Trim simple whitespace
    while (!data.empty() && (data.back() == '\n' || data.back() == '\r' || data.back() == ' ' || data.back() == '\t')) {
        data.pop_back();
    }
    return data;
}

void writeCachedStationId(const std::string& id) {
    if (id.empty()) return;
    std::filesystem::create_directories("cache");
    std::ofstream f("cache/station_id.txt", std::ios::binary);
    if (f.good()) {
        f << id;
    }
}

float getNumber(const vjson::Value& obj, const std::string& key, float def = 0.0f) {
    if (!obj.has(key)) return def;
    return obj[key].asFloat(def);
}

bool authorized(const httplib::Request& req, const std::string& apiKey) {
    if (apiKey.empty()) return true;
    auto headerKey = req.get_header_value("X-Api-Key", "");
    if (headerKey == apiKey) return true;
    auto auth = req.get_header_value("Authorization", "");
    const std::string bearer = "Bearer ";
    if (auth.rfind(bearer, 0) == 0) {
        return auth.substr(bearer.size()) == apiKey;
    }
    return false;
}

std::string stateJson(const audio::PublicState& state) {
    std::stringstream ss;
    ss << "{";
    ss << "\"mood\":\"" << escapeJson(state.moodId) << "\",";
    ss << "\"targetMood\":\"" << escapeJson(state.targetMoodId) << "\",";
    ss << "\"energy\":" << state.energy << ",";
    ss << "\"intensity\":" << state.intensity << ",";
    ss << "\"activity\":" << state.activity << ",";
    ss << "\"idleSeconds\":" << state.idleSeconds << ",";
    ss << "\"playing\":" << (state.playing ? "true" : "false") << ",";
    ss << "\"activeProcess\":\"" << escapeJson(state.activeProcess) << "\",";
    ss << "\"updatedAtMs\":" << state.updatedAtMs;
    ss << "}";
    return ss.str();
}

float timeOfDay01() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &tt);
#else
    local_tm = *std::localtime(&tt);
#endif
    int secs = local_tm.tm_hour * 3600 + local_tm.tm_min * 60 + local_tm.tm_sec;
    return static_cast<float>(secs) / 86400.0f;
}

std::string vibeJson(const audio::PublicState& state) {
    std::stringstream ss;
    ss << "{";
    ss << "\"mood\":\"" << escapeJson(state.moodId) << "\",";
    ss << "\"energy\":" << state.energy << ",";
    ss << "\"activity\":" << state.activity << ",";
    ss << "\"intensity\":" << state.intensity << ",";
    ss << "\"timeOfDay\":" << timeOfDay01();
    ss << "}";
    return ss.str();
}

std::string stationPayloadJson(const StationConfig& cfg,
                               const audio::PublicState& state,
                               const std::string& stationId,
                               bool broadcasting,
                               const std::string& sessionId) {
    std::stringstream ss;
    ss << "{";
    bool first = true;

    auto addField = [&](const std::string& key, const std::string& value) {
        if (!first) ss << ",";
        first = false;
        ss << "\"" << key << "\":\"" << escapeJson(value) << "\"";
    };
    auto addNumber = [&](const std::string& key, float value) {
        if (!first) ss << ",";
        first = false;
        ss << "\"" << key << "\":" << value;
    };
    auto addBool = [&](const std::string& key, bool value) {
        if (!first) ss << ",";
        first = false;
        ss << "\"" << key << "\":" << (value ? "true" : "false");
    };

    if (!stationId.empty()) {
        addField("id", stationId);
    }
    addField("name", cfg.name);
    addField("region", cfg.region);
    addNumber("frequency", cfg.frequency);
    if (!cfg.description.empty()) addField("description", cfg.description);
    if (!cfg.streamUrl.empty()) addField("streamUrl", cfg.streamUrl);
    std::string status = broadcasting ? "live" : (state.playing ? "idle" : "offline");
    addField("status", status);
    addBool("broadcasting", broadcasting);
    if (!sessionId.empty()) addField("sessionId", sessionId);
    addField("mood", state.moodId);
    addNumber("energy", state.energy);
    addBool("playing", state.playing);

    ss << "}";
    return ss.str();
}
} // namespace

WebServer::WebServer(audio::Engine& engine, int port)
    : engine_(engine), port_(port), running_(false) {}

WebServer::~WebServer() {
    stop();
}

bool WebServer::start() {
    if (running_) return true;

    running_ = true;
    loadStationConfig();
    serverThread_ = std::thread(&WebServer::run, this);
    registryThread_ = std::thread(&WebServer::runRegistryClient, this);
    wsServer_ = std::make_unique<WsServer>([this]() {
        auto state = engine_.snapshot();
        return stateJson(state);
    }, port_ + 1, bridgeApiKey_);
    wsServer_->start();
    util::logInfo("WebServer: WS started on port " + std::to_string(port_ + 1));
    
    util::logInfo("WebServer: Started on port " + std::to_string(port_));
    return true;
}

void WebServer::stop() {
    if (!running_) return;
    
    running_ = false;
    // httplib server.stop() needs to be called from another thread or we just detach
    // Since we're using a blocking run() in a thread, we can just detach and let it die with the process 
    // or implement a proper stop via a client request if needed. 
    // For simplicity in this embedded use case:
    if (serverThread_.joinable()) {
        serverThread_.detach(); 
    }
    if (registryThread_.joinable()) {
        registryThread_.detach();
    }
    if (wsServer_) {
        wsServer_->stop();
    }
}

void WebServer::run() {
    httplib::Server svr;
    auto addCors = [&](httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Api-Key, X-Broadcast-Token");
    };
    auto requireAuth = [&](const httplib::Request& req, httplib::Response& res) {
        if (authorized(req, bridgeApiKey_)) return true;
        res.status = 401;
        res.set_content("{\"error\":\"unauthorized\"}", "application/json");
        addCors(res);
        return false;
    };
    auto validateToken = [&](const std::string& token, TokenPayload& payload) {
        if (token.empty()) return false;
        if (!parseToken(token, payload, broadcastSecret_)) return false;
        if (payload.stationId != stationId_) return false;
        if (nowMs() > payload.expiresAt) return false;
        return true;
    };

    svr.Options(R"(/api/.*)", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        addCors(res);
        res.status = 204;
    });

    // API Endpoint: Get Current State
    svr.Get("/api/state", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        auto state = engine_.snapshot();
        res.set_content(stateJson(state), "application/json");
        addCors(res);
    });

    // Toggle Play/Pause
    svr.Post("/api/toggle", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        if (!requireAuth(req, res)) return;
        bool playing = !engine_.isPlaying();
        engine_.setPlaying(playing);
        res.set_content(makeJson("playing", playing ? "true" : "false"), "application/json");
        addCors(res);
    });

    // Set mood
    svr.Post("/api/mood", [&](const httplib::Request& req, httplib::Response& res) {
        if (!requireAuth(req, res)) return;
        std::string mood;
        if (!req.body.empty()) {
            auto parsed = vjson::parse(req.body);
            if (parsed.has_value() && parsed->isObject()) {
                const auto& root = *parsed;
                if (root.has("mood")) mood = root["mood"].asString("");
                if (mood.empty() && root.has("id")) mood = root["id"].asString("");
            }
        }
        if (mood.empty()) {
            res.status = 400;
            res.set_content(makeJson("error", "missing mood"), "application/json");
            addCors(res);
            return;
        }
        engine_.setMood(mood);
        auto state = engine_.snapshot();
        res.set_content(stateJson(state), "application/json");
        addCors(res);
    });

    // Vibe vector (privacy-safe)
    svr.Get("/api/vibe", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        auto state = engine_.snapshot();
        res.set_content(vibeJson(state), "application/json");
        addCors(res);
    });

    // Broadcast token stub
    svr.Post("/api/broadcast/token", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        if (!requireAuth(req, res)) return;
        uint64_t expiry = nowMs() + 10 * 60 * 1000;
        std::string token = issueToken(stationId_, expiry, broadcastSecret_);
        {
            std::lock_guard<std::mutex> lock(broadcastMutex_);
            broadcastTokenExpiryMs_ = expiry;
        }
        std::stringstream ss;
        ss << "{";
        ss << "\"token\":\"" << token << "\",";
        ss << "\"expiresInMs\":600000,";
        ss << "\"expiresAtMs\":" << expiry;
        ss << "}";
        res.set_content(ss.str(), "application/json");
        addCors(res);
    });

    // Broadcast ingest info
    svr.Get("/api/broadcast/ingest", [&](const httplib::Request& req, httplib::Response& res) {
        if (!requireAuth(req, res)) return;
        std::string token = req.get_header_value("X-Broadcast-Token", "");
        if (token.empty()) {
            auto auth = req.get_header_value("Authorization", "");
            const std::string bearer = "Bearer ";
            if (auth.rfind(bearer, 0) == 0) {
                token = auth.substr(bearer.size());
            }
        }
        bool valid = false;
        bool broadcasting = false;
        std::string sessionId;
        uint64_t startedAt = 0;
        {
            std::lock_guard<std::mutex> lock(broadcastMutex_);
            TokenPayload payload;
            valid = validateToken(token, payload);
            broadcasting = broadcasting_;
            sessionId = broadcastSessionId_;
            startedAt = broadcastStartedMs_;
        }
        if (!valid) {
            res.status = 401;
            res.set_content("{\"error\":\"invalid_token\"}", "application/json");
            addCors(res);
            return;
        }
        std::string rtmpBase = getEnvVar("KEEGAN_RTMP_URL");
        if (rtmpBase.empty()) rtmpBase = "rtmp://localhost/live";
        std::string hlsBase = getEnvVar("KEEGAN_HLS_URL");
        if (hlsBase.empty()) hlsBase = "http://localhost:8888/live";
        std::string webrtcBase = getEnvVar("KEEGAN_WEBRTC_URL");
        if (webrtcBase.empty()) webrtcBase = "http://localhost:8889/live";

        std::stringstream ss;
        ss << "{";
        ss << "\"broadcasting\":" << (broadcasting ? "true" : "false") << ",";
        ss << "\"sessionId\":\"" << sessionId << "\",";
        ss << "\"startedAtMs\":" << startedAt << ",";
        ss << "\"protocols\":[\"webrtc\",\"rtmp\",\"hls\"],";
        ss << "\"webrtcUrl\":\"" << webrtcBase << "/" << token << "\",";
        ss << "\"rtmpUrl\":\"" << rtmpBase << "/" << token << "\",";
        ss << "\"hlsUrl\":\"" << hlsBase << "/" << token << "/index.m3u8\"";
        ss << "}";
        res.set_content(ss.str(), "application/json");
        addCors(res);
    });

    // Broadcast start stub
    svr.Post("/api/broadcast/start", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        if (!requireAuth(req, res)) return;
        std::string token;
        std::string streamUrl;
        if (!req.body.empty()) {
            auto parsed = vjson::parse(req.body);
            if (parsed.has_value() && parsed->isObject()) {
                const auto& root = *parsed;
                if (root.has("token")) token = root["token"].asString("");
                if (root.has("streamUrl")) streamUrl = root["streamUrl"].asString("");
            }
        }
        if (token.empty()) {
            token = req.get_header_value("X-Broadcast-Token", "");
        }
        if (token.empty()) {
            auto auth = req.get_header_value("Authorization", "");
            const std::string bearer = "Bearer ";
            if (auth.rfind(bearer, 0) == 0) {
                token = auth.substr(bearer.size());
            }
        }
        bool valid = false;
        {
            std::lock_guard<std::mutex> lock(broadcastMutex_);
            TokenPayload payload;
            valid = validateToken(token, payload);
            if (valid) {
                broadcasting_ = true;
                broadcastStartedMs_ = nowMs();
                broadcastUpdatedMs_ = broadcastStartedMs_;
                broadcastSessionId_ = "sess_" + randomHex(10);
            }
        }
        if (!valid) {
            res.status = 401;
            res.set_content("{\"error\":\"invalid_token\"}", "application/json");
            addCors(res);
            return;
        }
        if (streamUrl.empty()) {
            streamUrl = "http://localhost:8888/live/" + token + "/index.m3u8";
        }
        {
            std::lock_guard<std::mutex> lock(stationMutex_);
            stationConfig_.streamUrl = streamUrl;
        }
        util::Telemetry::instance().record("broadcast_start", {
            {"stationId", stationId_},
            {"sessionId", broadcastSessionId_}
        });
        std::stringstream ss;
        ss << "{";
        ss << "\"broadcasting\":true,";
        ss << "\"sessionId\":\"" << broadcastSessionId_ << "\",";
        ss << "\"startedAtMs\":" << broadcastStartedMs_ << ",";
        ss << "\"streamUrl\":\"" << streamUrl << "\"";
        ss << "}";
        res.set_content(ss.str(), "application/json");
        addCors(res);
    });

    // Broadcast stop stub
    svr.Post("/api/broadcast/stop", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        if (!requireAuth(req, res)) return;
        std::string token;
        if (!req.body.empty()) {
            auto parsed = vjson::parse(req.body);
            if (parsed.has_value() && parsed->isObject()) {
                const auto& root = *parsed;
                if (root.has("token")) token = root["token"].asString("");
            }
        }
        if (token.empty()) {
            token = req.get_header_value("X-Broadcast-Token", "");
        }
        if (token.empty()) {
            auto auth = req.get_header_value("Authorization", "");
            const std::string bearer = "Bearer ";
            if (auth.rfind(bearer, 0) == 0) {
                token = auth.substr(bearer.size());
            }
        }
        bool valid = false;
        {
            std::lock_guard<std::mutex> lock(broadcastMutex_);
            TokenPayload payload;
            valid = validateToken(token, payload);
            if (valid) {
                broadcasting_ = false;
                broadcastUpdatedMs_ = nowMs();
            }
        }
        if (!valid) {
            res.status = 401;
            res.set_content("{\"error\":\"invalid_token\"}", "application/json");
            addCors(res);
            return;
        }
        util::Telemetry::instance().record("broadcast_stop", {
            {"stationId", stationId_},
            {"sessionId", broadcastSessionId_}
        });
        res.set_content("{\"broadcasting\":false}", "application/json");
        addCors(res);
    });

    // Broadcast status
    svr.Get("/api/broadcast/status", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        if (!requireAuth(req, res)) return;
        bool broadcasting = false;
        std::string sessionId;
        uint64_t startedAt = 0;
        uint64_t updatedAt = 0;
        uint64_t tokenExpiry = 0;
        std::string streamUrl;
        {
            std::lock_guard<std::mutex> lock(broadcastMutex_);
            broadcasting = broadcasting_;
            sessionId = broadcastSessionId_;
            startedAt = broadcastStartedMs_;
            updatedAt = broadcastUpdatedMs_;
            tokenExpiry = broadcastTokenExpiryMs_;
        }
        {
            std::lock_guard<std::mutex> lock(stationMutex_);
            streamUrl = stationConfig_.streamUrl;
        }
        std::stringstream ss;
        ss << "{";
        ss << "\"broadcasting\":" << (broadcasting ? "true" : "false") << ",";
        ss << "\"sessionId\":\"" << sessionId << "\",";
        ss << "\"startedAtMs\":" << startedAt << ",";
        ss << "\"updatedAtMs\":" << updatedAt << ",";
        ss << "\"tokenExpiresAtMs\":" << tokenExpiry << ",";
        ss << "\"streamUrl\":\"" << streamUrl << "\"";
        ss << "}";
        res.set_content(ss.str(), "application/json");
        addCors(res);
    });

    // Health
    svr.Get("/api/health", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        res.set_content("{\"status\":\"ok\"}", "application/json");
        addCors(res);
    });

    // Deprecated SSE endpoint
    svr.Get("/api/events", [&](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        std::string wsUrl = "ws://localhost:" + std::to_string(port_ + 1) + "/events";
        res.status = 410;
        std::string payload = "{\"error\":\"sse_removed\",\"ws\":\"" + wsUrl + "\"}";
        res.set_content(payload, "application/json");
        addCors(res);
    });

    // Serve Static Files (React App)
    // Assumes "web/dist" exists relative to CWD
    auto ret = svr.set_mount_point("/", "./web/dist");
    if (!ret) {
        util::logWarn("WebServer: Failed to mount ./web/dist");
    }

    // Fallback for SPA routing (serve index.html for unknown paths)
    svr.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        if (req.method == "GET" && req.path.find("/api") == std::string::npos) {
            std::ifstream f("./web/dist/index.html");
            if (f.good()) {
                std::stringstream buffer;
                buffer << f.rdbuf();
                res.set_content(buffer.str(), "text/html");
                return;
            }
        }
        res.status = 404;
        res.set_content("Not Found", "text/plain");
    });

    util::logInfo("WebServer: Listening...");
    svr.listen("0.0.0.0", port_);
}

void WebServer::loadStationConfig() {
    StationConfig cfg;
    auto raw = readTextFile("config/station.json");
    if (!raw.empty()) {
        auto parsed = vjson::parse(raw);
        if (parsed.has_value() && parsed->isObject()) {
            const auto& root = *parsed;
            if (root.has("id")) cfg.id = root["id"].asString("");
            if (root.has("name")) cfg.name = root["name"].asString(cfg.name);
            if (root.has("region")) cfg.region = root["region"].asString(cfg.region);
            cfg.frequency = getNumber(root, "frequency", cfg.frequency);
            if (root.has("description")) cfg.description = root["description"].asString(cfg.description);
            if (root.has("streamUrl")) cfg.streamUrl = root["streamUrl"].asString("");
            if (root.has("registryUrl")) cfg.registryUrl = root["registryUrl"].asString(cfg.registryUrl);
        }
    }

    // Environment overrides
    auto envRegistry = getEnvVar("KEEGAN_REGISTRY_URL");
    if (!envRegistry.empty()) cfg.registryUrl = envRegistry;
    auto envName = getEnvVar("KEEGAN_STATION_NAME");
    if (!envName.empty()) cfg.name = envName;
    auto envRegion = getEnvVar("KEEGAN_STATION_REGION");
    if (!envRegion.empty()) cfg.region = envRegion;
    auto envDesc = getEnvVar("KEEGAN_STATION_DESCRIPTION");
    if (!envDesc.empty()) cfg.description = envDesc;
    auto envStream = getEnvVar("KEEGAN_STREAM_URL");
    if (!envStream.empty()) cfg.streamUrl = envStream;
    auto envFreq = getEnvVar("KEEGAN_STATION_FREQUENCY");
    if (!envFreq.empty()) {
        try {
            cfg.frequency = std::stof(envFreq);
        } catch (...) {
            // ignore
        }
    }

    if (cfg.id.empty()) {
        cfg.id = readCachedStationId();
    }
    if (cfg.id.empty()) {
        cfg.id = "st_local_" + randomHex(6);
        writeCachedStationId(cfg.id);
    }
    stationConfig_ = cfg;
    stationId_ = cfg.id;

    bridgeApiKey_ = getEnvVar("KEEGAN_BRIDGE_KEY");
    registryApiKey_ = getEnvVar("KEEGAN_REGISTRY_KEY");
    broadcastSecret_ = getEnvVar("KEEGAN_BROADCAST_SECRET");
    if (broadcastSecret_.empty()) {
        broadcastSecret_ = bridgeApiKey_;
    }
    if (broadcastSecret_.empty()) {
        broadcastSecret_ = "dev_secret";
    }
}

void WebServer::runRegistryClient() {
    std::string registryUrl;
    {
        std::lock_guard<std::mutex> lock(stationMutex_);
        registryUrl = stationConfig_.registryUrl;
    }
    if (registryUrl.empty()) {
        util::logWarn("Registry: registryUrl not set, skipping registration");
        return;
    }

    httplib::Client cli(registryUrl.c_str());
    cli.set_connection_timeout(2, 0);
    cli.set_read_timeout(5, 0);
    cli.set_write_timeout(5, 0);

    auto pushUpdate = [&]() {
        auto state = engine_.snapshot();
        StationConfig cfg;
        {
            std::lock_guard<std::mutex> lock(stationMutex_);
            cfg = stationConfig_;
        }
        bool broadcasting = false;
        std::string sessionId;
        {
            std::lock_guard<std::mutex> lock(broadcastMutex_);
            broadcasting = broadcasting_;
            sessionId = broadcastSessionId_;
        }

        std::string payload = stationPayloadJson(cfg, state, stationId_, broadcasting, sessionId);
        httplib::Headers headers;
        if (!registryApiKey_.empty()) {
            headers.emplace("X-Api-Key", registryApiKey_);
        }
        auto res = cli.Post("/api/stations", headers, payload, "application/json");
        if (!res) {
            util::logWarn("Registry: failed to reach registry server");
            return;
        }
        if (res->status != 200) {
            util::logWarn("Registry: registry rejected update (status " + std::to_string(res->status) + ")");
            return;
        }
        auto parsed = vjson::parse(res->body);
        if (parsed.has_value() && parsed->isObject()) {
            const auto& root = *parsed;
            if (root.has("id")) {
                std::string id = root["id"].asString("");
                if (!id.empty() && id != stationId_) {
                    stationId_ = id;
                    writeCachedStationId(id);
                    util::logInfo("Registry: assigned station id " + id);
                }
            }
        }
    };

    pushUpdate();
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(15));
        if (!running_) break;
        pushUpdate();
    }
}

} // namespace uisrv
