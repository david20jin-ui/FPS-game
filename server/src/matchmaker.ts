import { randomUUID } from "node:crypto";
import type { Server, Socket } from "socket.io";
import { GameServerPool } from "./gameServerPool.js";
import type {
  MatchFoundPayload,
  MatchMode,
  QueuedPlayer,
  QueueJoinPayload,
} from "./types.js";

export class Matchmaker {
  private queues: Record<MatchMode, QueuedPlayer[]> = { solo: [], team: [] };
  private required: Record<MatchMode, number>;
  private pool = new GameServerPool();

  constructor(
    private readonly io: Server,
    options: { soloTeamSize?: number; teamTeamSize?: number } = {}
  ) {
    this.required = {
      solo: options.soloTeamSize ?? 1,
      team: options.teamTeamSize ?? 2,
    };
  }

  shutdown(): void { this.pool.shutdown(); }

  join(socket: Socket, payload: QueueJoinPayload): void {
    this.removeBySocket(socket.id);
    const entry: QueuedPlayer = {
      socketId: socket.id,
      playerId: payload.playerId,
      name: payload.name,
      mode: payload.mode,
      joinedAt: Date.now(),
    };
    this.queues[payload.mode].push(entry);
    this.broadcastQueue(payload.mode);
    this.tryForm(payload.mode);
  }

  leave(socketId: string): void {
    const removed = this.removeBySocket(socketId);
    if (removed) this.broadcastQueue(removed.mode);
  }

  private removeBySocket(socketId: string): QueuedPlayer | undefined {
    for (const mode of ["solo", "team"] as MatchMode[]) {
      const q = this.queues[mode];
      const idx = q.findIndex((p) => p.socketId === socketId);
      if (idx >= 0) {
        const [removed] = q.splice(idx, 1);
        return removed;
      }
    }
    return undefined;
  }

  private broadcastQueue(mode: MatchMode): void {
    const q = this.queues[mode];
    q.forEach((player, i) => {
      this.io.to(player.socketId).emit("queue:update", {
        position: i + 1,
        playersInQueue: q.length,
        requiredPlayers: this.required[mode],
      });
    });
  }

  private tryForm(mode: MatchMode): void {
    const q = this.queues[mode];
    const needed = this.required[mode];
    while (q.length >= needed) {
      const group = q.splice(0, needed);
      const matchId = randomUUID();
      const { ip, port } = this.pool.allocate(matchId, mode);
      const payload: MatchFoundPayload = {
        matchId,
        ip,
        port,
        token: randomUUID(),
        mode,
      };
      for (const player of group) {
        this.io.to(player.socketId).emit("match:found", payload);
      }
      console.log(
        `[matchmaker] match formed (${mode}) for ${group.map((p) => p.name).join(", ")} -> ${ip}:${port}`
      );
    }
    this.broadcastQueue(mode);
  }
}
