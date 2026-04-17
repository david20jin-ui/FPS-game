export type MatchMode = "solo" | "team";
export type Difficulty = "easy" | "medium" | "hard";
export type MapId = "range" | "warehouse" | "arena";

export interface MapInfo {
  id: MapId;
  name: string;
  description: string;
}

export const MAPS: MapInfo[] = [
  { id: "range",     name: "The Range",  description: "Symmetric practice map with central cover and corner walls. Great for warmup." },
  { id: "warehouse", name: "Warehouse",  description: "Long corridor with four pillars and corner crate stacks. Rewards positioning." },
  { id: "arena",     name: "Arena",      description: "Open square arena with a central raised platform and perimeter cover. Lots of angles." },
];

export interface DifficultyInfo {
  id: Difficulty;
  name: string;
  description: string;
  accent: string;
}

export const DIFFICULTIES: DifficultyInfo[] = [
  { id: "easy",   name: "Easy",   description: "Slow bots with Classic pistols, generous reaction time.", accent: "#3ddc84" },
  { id: "medium", name: "Medium", description: "Bots with Spectre SMGs, balanced reflexes.",             accent: "#f2b84b" },
  { id: "hard",   name: "Hard",   description: "Vandal-wielding bots with snappy aim. Tactical only.",   accent: "#ff4655" },
];

export interface QueueJoinPayload {
  playerId: string;
  name: string;
  mode: MatchMode;
}

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
