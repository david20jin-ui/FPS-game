# Wire Protocol

## 1. Launcher ↔ Matchmaking Server (Socket.io, port 4000)

### Client → Server

| Event         | Payload                                                     |
| ------------- | ----------------------------------------------------------- |
| `queue:join`  | `{ playerId, name, mode: "solo" | "team" }`                 |
| `queue:leave` | `{}`                                                        |

### Server → Client

| Event          | Payload                                                                 |
| -------------- | ----------------------------------------------------------------------- |
| `queue:update` | `{ position, playersInQueue, requiredPlayers }`                         |
| `match:found`  | `{ matchId, ip, port, token, mode }`                                    |
| `error`        | `{ code, message }`                                                     |

## 2. Launcher → Game (CLI)

```
fps_game --server=<ip>:<port> --token=<t> --name=<n> [--mode=solo|team] [--fov=90] [--sensitivity=0.4]
```

## 3. Game ↔ Dedicated Game Server (Phase 7)

- Transport: UDP via enet, default port 7777.
- Tick rate: 64 Hz.
- Packet structs are defined in `game/src/net/Protocol.h`.
