#pragma once

#include "Protocol.h"
#include "../sim/Input.h"

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

struct _ENetHost;
struct _ENetPeer;

namespace fps::net {

struct ConnectedClient {
    uint8_t     clientId;
    _ENetPeer*  peer;
    std::string name;
    uint32_t    lastAck = 0;
    std::deque<PlayerInput> pending;
};

class NetServer {
public:
    static bool globalInit();
    static void globalShutdown();

    NetServer();
    ~NetServer();

    bool listen(uint16_t port, uint8_t maxClients = 10);
    void close();

    void poll();
    void broadcastSnapshot(const SnapshotPacket& snap);

    std::vector<ConnectedClient*> clients();
    ConnectedClient* findByPeer(_ENetPeer* peer);

private:
    _ENetHost* host_ = nullptr;
    std::unordered_map<_ENetPeer*, ConnectedClient> clientsMap_;
    uint8_t nextId_ = 0;
};

} // namespace fps::net
