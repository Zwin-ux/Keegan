#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace uisrv {

class WsServer {
public:
    using PayloadProvider = std::function<std::string()>;

    WsServer(PayloadProvider provider, int port = 3001, std::string authToken = {});
    ~WsServer();

    bool start();
    void stop();

    int port() const { return port_; }

private:
#ifdef _WIN32
    using SocketHandle = SOCKET;
    static constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
    using SocketHandle = int;
    static constexpr SocketHandle kInvalidSocket = -1;
#endif

    PayloadProvider payloadProvider_;
    int port_;
    std::atomic<bool> running_;
    std::thread acceptThread_;
    std::thread broadcastThread_;
    std::string authToken_;

    SocketHandle listenSock_ = kInvalidSocket;
    std::mutex clientsMutex_;
    std::vector<SocketHandle> clients_;

    void acceptLoop();
    void broadcastLoop();
    bool handshake(SocketHandle clientSock);
    void removeClient(SocketHandle clientSock);
    void closeSocket(SocketHandle sock);
};

} // namespace uisrv
