#include "ws_server.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>

namespace uisrv {

namespace {
constexpr const char* kWsGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

std::string base64Encode(const unsigned char* data, size_t len) {
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < len) {
        unsigned int v = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out.push_back(table[(v >> 18) & 0x3F]);
        out.push_back(table[(v >> 12) & 0x3F]);
        out.push_back(table[(v >> 6) & 0x3F]);
        out.push_back(table[v & 0x3F]);
        i += 3;
    }

    if (i < len) {
        unsigned int v = data[i] << 16;
        if (i + 1 < len) v |= (data[i + 1] << 8);

        out.push_back(table[(v >> 18) & 0x3F]);
        out.push_back(table[(v >> 12) & 0x3F]);
        if (i + 1 < len) {
            out.push_back(table[(v >> 6) & 0x3F]);
            out.push_back('=');
        } else {
            out.push_back('=');
            out.push_back('=');
        }
    }

    return out;
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

std::string sha1Base64(const std::string& input) {
    Sha1 sha;
    sha.update(reinterpret_cast<const unsigned char*>(input.data()), input.size());
    auto digest = sha.final();
    return base64Encode(digest.data(), digest.size());
}

} // namespace

WsServer::WsServer(PayloadProvider provider, int port, std::string authToken)
    : payloadProvider_(std::move(provider)),
      port_(port),
      running_(false),
      authToken_(std::move(authToken)) {}

WsServer::~WsServer() {
    stop();
}

bool WsServer::start() {
    if (running_) return true;
    running_ = true;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        running_ = false;
        return false;
    }
#endif

    acceptThread_ = std::thread(&WsServer::acceptLoop, this);
    broadcastThread_ = std::thread(&WsServer::broadcastLoop, this);
    return true;
}

void WsServer::stop() {
    if (!running_) return;
    running_ = false;

    if (listenSock_ != kInvalidSocket) {
        closeSocket(listenSock_);
        listenSock_ = kInvalidSocket;
    }

    if (acceptThread_.joinable()) acceptThread_.join();
    if (broadcastThread_.joinable()) broadcastThread_.join();

    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto sock : clients_) {
        closeSocket(sock);
    }
    clients_.clear();

#ifdef _WIN32
    WSACleanup();
#endif
}

void WsServer::acceptLoop() {
    listenSock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock_ == kInvalidSocket) {
        running_ = false;
        return;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(listenSock_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(listenSock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
#ifdef _WIN32
    addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
#else
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
#endif

    if (bind(listenSock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        closeSocket(listenSock_);
        listenSock_ = kInvalidSocket;
        running_ = false;
        return;
    }

    if (listen(listenSock_, 8) < 0) {
        closeSocket(listenSock_);
        listenSock_ = kInvalidSocket;
        running_ = false;
        return;
    }

    while (running_) {
        sockaddr_in clientAddr{};
#ifdef _WIN32
        int len = sizeof(clientAddr);
#else
        socklen_t len = sizeof(clientAddr);
#endif
        SocketHandle clientSock = accept(listenSock_, reinterpret_cast<sockaddr*>(&clientAddr), &len);
        if (!running_) break;
        if (clientSock == kInvalidSocket) {
            continue;
        }

        if (!handshake(clientSock)) {
            closeSocket(clientSock);
            continue;
        }

        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.push_back(clientSock);
    }
}

void WsServer::broadcastLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (!running_) break;

        std::string payload = payloadProvider_ ? payloadProvider_() : std::string();
        if (payload.empty()) {
            continue;
        }

        // Build WS frame (text, unmasked).
        std::string frame;
        frame.reserve(payload.size() + 16);
        frame.push_back(static_cast<char>(0x81));

        if (payload.size() < 126) {
            frame.push_back(static_cast<char>(payload.size()));
        } else if (payload.size() <= 0xFFFF) {
            frame.push_back(static_cast<char>(126));
            frame.push_back(static_cast<char>((payload.size() >> 8) & 0xFF));
            frame.push_back(static_cast<char>(payload.size() & 0xFF));
        } else {
            frame.push_back(static_cast<char>(127));
            for (int i = 7; i >= 0; --i) {
                frame.push_back(static_cast<char>((payload.size() >> (i * 8)) & 0xFF));
            }
        }
        frame.append(payload);

        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.erase(std::remove_if(clients_.begin(), clients_.end(), [&](SocketHandle sock) {
            int sent = send(sock, frame.data(), static_cast<int>(frame.size()), 0);
            if (sent <= 0) {
                closeSocket(sock);
                return true;
            }
            return false;
        }), clients_.end());
    }
}

bool WsServer::handshake(SocketHandle clientSock) {
    std::array<char, 2048> buffer{};
    int received = recv(clientSock, buffer.data(), static_cast<int>(buffer.size() - 1), 0);
    if (received <= 0) return false;

    std::string request(buffer.data(), static_cast<size_t>(received));
    std::string requestLine;
    auto firstLineEnd = request.find("\r\n");
    if (firstLineEnd != std::string::npos) {
        requestLine = request.substr(0, firstLineEnd);
    }

    std::string path;
    if (!requestLine.empty()) {
        auto start = requestLine.find(' ');
        if (start != std::string::npos) {
            auto end = requestLine.find(' ', start + 1);
            if (end != std::string::npos) {
                path = requestLine.substr(start + 1, end - start - 1);
            }
        }
    }

    auto headerValue = [&](const std::string& key) -> std::string {
        std::string needle = key + ":";
        auto pos = request.find(needle);
        if (pos == std::string::npos) return "";
        pos += needle.size();
        auto end = request.find("\r\n", pos);
        if (end == std::string::npos) return "";
        std::string value = request.substr(pos, end - pos);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        return value;
    };

    if (!authToken_.empty()) {
        std::string token;
        if (!path.empty()) {
            auto qpos = path.find('?');
            if (qpos != std::string::npos) {
                std::string query = path.substr(qpos + 1);
                std::string key = "token=";
                auto tpos = query.find(key);
                if (tpos != std::string::npos) {
                    auto end = query.find('&', tpos);
                    token = query.substr(tpos + key.size(), end == std::string::npos ? std::string::npos : end - tpos - key.size());
                }
            }
        }
        if (token.empty()) {
            token = headerValue("X-Api-Key");
        }
        if (token.empty()) {
            auto auth = headerValue("Authorization");
            const std::string bearer = "Bearer ";
            if (auth.rfind(bearer, 0) == 0) {
                token = auth.substr(bearer.size());
            }
        }
        if (token != authToken_) {
            const char* resp = "HTTP/1.1 401 Unauthorized\r\n\r\n";
            send(clientSock, resp, static_cast<int>(std::strlen(resp)), 0);
            return false;
        }
    }
    auto keyPos = request.find("Sec-WebSocket-Key:");
    if (keyPos == std::string::npos) return false;

    keyPos += std::strlen("Sec-WebSocket-Key:");
    auto endPos = request.find("\r\n", keyPos);
    if (endPos == std::string::npos) return false;

    std::string key = request.substr(keyPos, endPos - keyPos);
    // Trim whitespace
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);

    std::string accept = sha1Base64(key + kWsGuid);

    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";

    int sent = send(clientSock, response.c_str(), static_cast<int>(response.size()), 0);
    return sent > 0;
}

void WsServer::removeClient(SocketHandle clientSock) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    clients_.erase(std::remove(clients_.begin(), clients_.end(), clientSock), clients_.end());
    closeSocket(clientSock);
}

void WsServer::closeSocket(SocketHandle sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

} // namespace uisrv
