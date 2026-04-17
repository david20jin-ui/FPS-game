#include "NetClient.h"

#include <enet/enet.h>

#include <cstdio>
#include <cstring>

namespace fps::net {

bool NetClient::globalInit() { return enet_initialize() == 0; }
void NetClient::globalShutdown() { enet_deinitialize(); }

NetClient::NetClient() = default;
NetClient::~NetClient() { disconnect(); }

bool NetClient::connect(const std::string& host, uint16_t port,
                        const std::string& playerName, uint32_t timeoutMs) {
    host_ = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!host_) return false;
    ENetAddress addr{};
    enet_address_set_host(&addr, host.c_str());
    addr.port = port;
    peer_ = enet_host_connect(host_, &addr, 2, 0);
    if (!peer_) return false;

    ENetEvent ev{};
    if (enet_host_service(host_, &ev, timeoutMs) > 0 &&
        ev.type == ENET_EVENT_TYPE_CONNECT) {
        connected_ = true;
        HelloPacket hp{};
        std::snprintf(hp.name, sizeof(hp.name), "%s", playerName.c_str());
        ENetPacket* pkt = enet_packet_create(&hp, sizeof(hp), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer_, 0, pkt);
        enet_host_flush(host_);
        return true;
    }
    enet_peer_reset(peer_);
    peer_ = nullptr;
    return false;
}

void NetClient::disconnect() {
    if (peer_) { enet_peer_disconnect_now(peer_, 0); peer_ = nullptr; }
    if (host_) { enet_host_destroy(host_); host_ = nullptr; }
    connected_ = false;
}

void NetClient::sendInput(const PlayerInput& in) {
    if (!connected_ || !peer_) return;
    InputPacket p{};
    p.input = in;
    ENetPacket* pkt = enet_packet_create(&p, sizeof(p), 0);
    enet_peer_send(peer_, 1, pkt);
}

void NetClient::sendFire(const FirePacket& fp) {
    if (!connected_ || !peer_) return;
    ENetPacket* pkt = enet_packet_create(&fp, sizeof(fp), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer_, 1, pkt);
}

void NetClient::poll() {
    if (!host_) return;
    ENetEvent ev{};
    while (enet_host_service(host_, &ev, 0) > 0) {
        switch (ev.type) {
        case ENET_EVENT_TYPE_RECEIVE: {
            if (ev.packet->dataLength < 1) { enet_packet_destroy(ev.packet); break; }
            uint8_t id = ev.packet->data[0];
            if (id == S2C_Welcome && ev.packet->dataLength >= sizeof(WelcomePacket)) {
                WelcomePacket wp;
                std::memcpy(&wp, ev.packet->data, sizeof(wp));
                clientId_ = wp.clientId;
                team_     = wp.team;
                if (welcomeH_) welcomeH_(wp);
            } else if (id == S2C_Snapshot && ev.packet->dataLength >= sizeof(SnapshotPacket)) {
                SnapshotPacket sp;
                std::memcpy(&sp, ev.packet->data, sizeof(sp));
                if (snapshotH_) snapshotH_(sp);
            } else if (id == S2C_HitFeedback && ev.packet->dataLength >= sizeof(HitFeedbackPacket)) {
                HitFeedbackPacket hp;
                std::memcpy(&hp, ev.packet->data, sizeof(hp));
                if (hitH_) hitH_(hp);
            }
            enet_packet_destroy(ev.packet);
            break;
        }
        case ENET_EVENT_TYPE_DISCONNECT:
            connected_ = false;
            break;
        default:
            break;
        }
    }
}

} // namespace fps::net
