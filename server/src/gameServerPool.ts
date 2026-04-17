import { spawn, ChildProcess } from "node:child_process";
import path from "node:path";
import fs from "node:fs";

/**
 * Allocates dedicated `fps_server` processes per match when enabled via
 * `FPS_SPAWN_GAME_SERVERS=1`. Ports are assigned from a pool starting at
 * `FPS_GAME_SERVER_PORT_BASE` (default 7777).
 */
export class GameServerPool {
  private next: number;
  private readonly base: number;
  private readonly binary: string;
  private readonly enabled: boolean;
  private readonly procs: Map<number, ChildProcess> = new Map();

  constructor() {
    this.base = Number(process.env.FPS_GAME_SERVER_PORT_BASE ?? 7777);
    this.next = this.base;
    this.binary =
      process.env.FPS_GAME_SERVER_BINARY ??
      path.resolve(process.cwd(), "..", "game", "build",
        process.platform === "win32" ? "fps_server.exe" : "fps_server");
    // Auto-spawn dedicated servers by default for team matches so 5v5 works
    // out of the box. Explicit disable: FPS_SPAWN_GAME_SERVERS=0.
    this.enabled = process.env.FPS_SPAWN_GAME_SERVERS !== "0";
  }

  allocate(
    matchId: string,
    mode: "solo" | "team",
    mapName?: string
  ): { ip: string; port: number } {
    const ip = process.env.GAME_SERVER_IP ?? "127.0.0.1";
    // Solo matches never need a real server; the client runs the whole sim.
    if (mode === "solo" || !this.enabled) {
      return { ip, port: Number(process.env.GAME_SERVER_PORT ?? 7777) };
    }
    if (!fs.existsSync(this.binary)) {
      console.warn(
        `[pool] fps_server binary not found at ${this.binary}; team match will fail to connect`
      );
      return { ip, port: Number(process.env.GAME_SERVER_PORT ?? 7777) };
    }
    const port = this.next++;
    const args = [`--server=${ip}:${port}`, `--mode=${mode}`, `--token=${matchId}`];
    if (mapName) args.push(`--map=${mapName}`);
    console.log(`[pool] spawning fps_server on :${port} for match ${matchId}`);
    const child = spawn(this.binary, args, { stdio: "inherit" });
    this.procs.set(port, child);
    child.on("exit", (code) => {
      console.log(`[pool] fps_server :${port} exited code=${code}`);
      this.procs.delete(port);
    });
    return { ip, port };
  }

  shutdown(): void {
    for (const [, p] of this.procs) p.kill();
    this.procs.clear();
  }
}
