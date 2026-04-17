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
    this.enabled = process.env.FPS_SPAWN_GAME_SERVERS === "1";
  }

  allocate(matchId: string, mode: "solo" | "team"): { ip: string; port: number } {
    const ip = process.env.GAME_SERVER_IP ?? "127.0.0.1";
    if (!this.enabled) {
      return { ip, port: Number(process.env.GAME_SERVER_PORT ?? 7777) };
    }
    if (!fs.existsSync(this.binary)) {
      console.warn(
        `[pool] FPS_SPAWN_GAME_SERVERS=1 but binary not found at ${this.binary}; falling back to mock`
      );
      return { ip, port: Number(process.env.GAME_SERVER_PORT ?? 7777) };
    }
    const port = this.next++;
    const child = spawn(
      this.binary,
      [`--server=${ip}:${port}`, `--mode=${mode}`, `--token=${matchId}`],
      { stdio: "inherit" }
    );
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
