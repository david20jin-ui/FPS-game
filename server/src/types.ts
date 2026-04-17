export type MatchMode = "solo" | "team";

export interface QueueJoinPayload {
  playerId: string;
  name: string;
  mode: MatchMode;
  mapId?: string;
}

export interface QueueLeavePayload {}

export interface QueueUpdatePayload {
  position: number;
  playersInQueue: number;
  requiredPlayers: number;
}

export interface MatchFoundPayload {
  matchId: string;
  ip: string;
  port: number;
  token: string;
  mode: MatchMode;
  mapId?: string;
}

export interface ErrorPayload {
  code: string;
  message: string;
}

export interface QueuedPlayer {
  socketId: string;
  playerId: string;
  name: string;
  mode: MatchMode;
  mapId?: string;
  joinedAt: number;
}
