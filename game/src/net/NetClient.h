#pragma once

#include "Protocol.h"
#include "../sim/Input.h"

#include <cstdint>
#include <functional>
#include <string>

struct _ENetHost;
struct _ENetPeer;

namespace fps::net {

class NetClient {
public:
    static bool globalInit();
    static void globalShutdown();

    using SnapshotHandler = std::function<void(const SnapshotPacket&)>;
    using WelcomeHandler  = std::function<void(const WelcomePacket&)>;

    NetClient();
    ~NetClient();
    NetClient(const NetClient&) = delete;
    NetClient& operator=(const NetClient&) = delete;

    bool connect(const std::string& host, uint16_t port,
                 const std::string& playerName, uint32_t timeoutMs = 5000);
    void disconnect();

    void sendInput(const PlayerInput& in);
    void poll();

    bool connected() const { return connected_; }
    uint8_t clientId() const { return clientId_; }

    void onSnapshot(SnapshotHandler h) { snapshotH_ = std::move(h); }
    void onWelcome(WelcomeHandler h)   { welcomeH_  = std::move(h); }

private:
    _ENetHost* host_ = nullptr;
    _ENetPeer* peer_ = nullptr;
    bool       connected_ = false;
    uint8_t    clientId_ = 0;
    SnapshotHandler snapshotH_;
    WelcomeHandler  welcomeH_;
};

} // namespace fps::net
