#include "NetServer.h"

#include <enet/enet.h>

#include <cstdio>
#include <cstring>

namespace fps::net {

bool NetServer::globalInit()    { return enet_initialize() == 0; }
void NetServer::globalShutdown() { enet_deinitialize(); }

NetServer::NetServer() = default;
NetServer::~NetServer() { close(); }

bool NetServer::listen(uint16_t port, uint8_t maxClients) {
    ENetAddress addr{};
    addr.host = ENET_HOST_ANY;
    addr.port = port;
    host_ = enet_host_create(&addr, maxClients, 2, 0, 0);
    return host_ != nullptr;
}

void NetServer::close() {
    if (host_) { enet_host_destroy(host_); host_ = nullptr; }
    clientsMap_.clear();
}

void NetServer::poll() {
    if (!host_) return;
    ENetEvent ev{};
    while (enet_host_service(host_, &ev, 0) > 0) {
        switch (ev.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            ConnectedClient c{};
            c.clientId = nextId_++;
            c.peer = ev.peer;
            clientsMap_[ev.peer] = c;
            std::printf("[net] client %u connected\n", c.clientId);
            break;
        }
        case ENET_EVENT_TYPE_RECEIVE: {
            auto it = clientsMap_.find(ev.peer);
            if (it == clientsMap_.end() || ev.packet->dataLength < 1) {
                enet_packet_destroy(ev.packet);
                break;
            }
            ConnectedClient& c = it->second;
            uint8_t id = ev.packet->data[0];
            if (id == C2S_Hello && ev.packet->dataLength >= sizeof(HelloPacket)) {
                HelloPacket hp;
                std::memcpy(&hp, ev.packet->data, sizeof(hp));
                c.name = std::string(hp.name, strnlen(hp.name, sizeof(hp.name)));
                WelcomePacket wp{};
                wp.clientId = c.clientId;
                wp.mapSeed = 0;
                ENetPacket* pkt = enet_packet_create(&wp, sizeof(wp), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(c.peer, 0, pkt);
                std::printf("[net] client %u name=%s welcomed\n", c.clientId, c.name.c_str());
            } else if (id == C2S_Input && ev.packet->dataLength >= sizeof(InputPacket)) {
                InputPacket in;
                std::memcpy(&in, ev.packet->data, sizeof(in));
                c.pending.push_back(in.input);
                while (c.pending.size() > 64) c.pending.pop_front();
            }
            enet_packet_destroy(ev.packet);
            break;
        }
        case ENET_EVENT_TYPE_DISCONNECT: {
            auto it = clientsMap_.find(ev.peer);
            if (it != clientsMap_.end()) {
                std::printf("[net] client %u disconnected\n", it->second.clientId);
                clientsMap_.erase(it);
            }
            break;
        }
        default: break;
        }
    }
}

void NetServer::broadcastSnapshot(const SnapshotPacket& snap) {
    if (!host_) return;
    ENetPacket* pkt = enet_packet_create(&snap, sizeof(snap), 0);
    enet_host_broadcast(host_, 1, pkt);
    enet_host_flush(host_);
}

std::vector<ConnectedClient*> NetServer::clients() {
    std::vector<ConnectedClient*> out;
    out.reserve(clientsMap_.size());
    for (auto& kv : clientsMap_) out.push_back(&kv.second);
    return out;
}

ConnectedClient* NetServer::findByPeer(_ENetPeer* peer) {
    auto it = clientsMap_.find(peer);
    return it == clientsMap_.end() ? nullptr : &it->second;
}

} // namespace fps::net
