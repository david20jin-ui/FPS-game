import { app, BrowserWindow, ipcMain } from "electron";
import { spawn } from "node:child_process";
import path from "node:path";
import fs from "node:fs";
import { fileURLToPath } from "node:url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

function resolveGameBinary(): string {
  const override = process.env.FPS_GAME_BINARY;
  if (override && fs.existsSync(override)) return override;

  const exeName = process.platform === "win32" ? "fps_game.exe" : "fps_game";
  const candidates = [
    path.resolve(__dirname, "..", "..", "game", "build", exeName),
    path.resolve(__dirname, "..", "..", "..", "game", "build", exeName),
    path.resolve(process.cwd(), "game", "build", exeName),
  ];
  for (const c of candidates) {
    if (fs.existsSync(c)) return c;
  }
  return candidates[0];
}

function createWindow(): void {
  const win = new BrowserWindow({
    width: 1280,
    height: 800,
    backgroundColor: "#0b0b10",
    webPreferences: {
      preload: path.join(__dirname, "preload.js"),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });

  const devUrl = process.env.VITE_DEV_SERVER_URL;
  if (devUrl) {
    win.loadURL(devUrl);
  } else {
    win.loadFile(path.join(__dirname, "..", "dist", "index.html"));
  }
}

ipcMain.handle(
  "fps:launchGame",
  async (_evt, args: { ip: string; port: number; token: string; name: string; mode: string; fov?: number; sensitivity?: number }) => {
    const binary = resolveGameBinary();
    if (!fs.existsSync(binary)) {
      return { ok: false, error: `Game binary not found at ${binary}. Build it with 'npm run build:game'.` };
    }
    const cliArgs = [
      `--server=${args.ip}:${args.port}`,
      `--token=${args.token}`,
      `--name=${args.name}`,
      `--mode=${args.mode}`,
    ];
    if (typeof args.fov === "number") cliArgs.push(`--fov=${args.fov}`);
    if (typeof args.sensitivity === "number") cliArgs.push(`--sensitivity=${args.sensitivity}`);
    const child = spawn(binary, cliArgs, { detached: true, stdio: "ignore" });
    child.unref();
    return { ok: true, pid: child.pid };
  }
);

app.whenReady().then(() => {
  createWindow();
  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit();
});
