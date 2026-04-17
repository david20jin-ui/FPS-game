#pragma once

#include "Protocol.h"
#include "../sim/Input.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct _ENetHost;
struct _ENetPeer;

namespace fps::net {

struct ConnectedClient {
    uint8_t     clientId;
    uint8_t     team = 0;
    _ENetPeer*  peer;
    std::string name;
    uint32_t    lastAck = 0;
    std::deque<PlayerInput> pending;
    std::deque<FirePacket>  pendingFires;
};

class NetServer {
public:
    static bool globalInit();
    static void globalShutdown();

    // Called for each client as it completes the handshake. Use this to
    // assign a team and compute the spawn position.
    using JoinHandler = std::function<void(ConnectedClient&)>;

    NetServer();
    ~NetServer();

    bool listen(uint16_t port, uint8_t maxClients = 10);
    void close();

    void poll();
    void broadcastSnapshot(const SnapshotPacket& snap);
    // Per-client snapshot (so we can customize `yourId` / `ackSequence`).
    void sendSnapshotTo(ConnectedClient& c, SnapshotPacket snap);
    void sendHitFeedback(ConnectedClient& c, const HitFeedbackPacket& hit);
    void sendWelcome(ConnectedClient& c);

    std::vector<ConnectedClient*> clients();
    ConnectedClient* findByPeer(_ENetPeer* peer);

    void onJoin(JoinHandler h) { joinH_ = std::move(h); }

private:
    _ENetHost* host_ = nullptr;
    std::unordered_map<_ENetPeer*, ConnectedClient> clientsMap_;
    uint8_t nextId_ = 0;
    JoinHandler joinH_;
};

} // namespace fps::net
