# Deploying to the Web

This doc walks through **Option A**: deploy the React matchmaking UI as a
public website on **Vercel**, and the Socket.io matchmaker on **Railway**.
Players download the native `fps_game` binary and run it locally; the website
handles queue + match-found handoff.

## What ends up where

| Piece | Hosted on | URL shape |
| --- | --- | --- |
| React launcher (web version) | Vercel | `https://yourgame.vercel.app` |
| Node/Socket.io matchmaker | Railway | `https://yourgame-mm.up.railway.app` |
| Native `fps_game` binary | GitHub Releases | downloaded once, runs locally |
| `fps_server` dedicated game server | Your VPS or local LAN host (see §4) | UDP port 7777 |

## 1. Deploy the matchmaker to Railway

1. Push your repo to GitHub (done).
2. Go to <https://railway.app> → **New Project** → **Deploy from GitHub repo**.
3. Pick `FPS-game`.
4. In the service settings:
   - **Root Directory**: `server`
   - **Start Command**: `node dist/index.js` (or leave blank — `railway.json` already sets it)
   - Railway auto-detects Node and runs `npm install && npm run build` via Nixpacks.
5. Click **Generate Domain**. Copy the URL, e.g. `https://fps-mm.up.railway.app`.
6. **Environment variables** (Railway → Variables tab):
   - `TEAM_TEAM_SIZE=2` while you test (lets matches form with just 2 players;
     bump to `10` later for real 5v5).
   - `FPS_SPAWN_GAME_SERVERS=0` (Railway doesn't expose UDP by default, so
     auto-spawning game servers on the Railway host won't help players).
   - `GAME_SERVER_IP=<your udp host>` — the public IP/DNS of the machine
     actually running `fps_server`. For testing, use your home public IP +
     port-forward, or a cheap VPS (see §4).
   - `GAME_SERVER_PORT=7777`.

Hit **Deploy**. After it's up, open `https://fps-mm.up.railway.app/health` in
your browser — you should see `{"ok":true}`.

## 2. Deploy the web launcher to Vercel

1. <https://vercel.com> → **Add New Project** → import the same GitHub repo.
2. **Framework preset**: Vite (auto-detected).
3. **Root Directory**: `launcher` — critical, this tells Vercel to build only
   the launcher workspace.
4. **Build command**: `npm run build:web` (already in `package.json`; skips
   the Electron bundle).
5. **Output directory**: `dist` (default).
6. **Environment variables**:
   - `VITE_MATCHMAKING_URL=https://fps-mm.up.railway.app` (from step 1).
   - `VITE_DOWNLOADS_BASE=https://github.com/<you>/FPS-game/releases/latest/download`.
   - `ELECTRON_SKIP_BINARY_DOWNLOAD=1` — stops Electron's postinstall from
     downloading the native binary during `npm install` in the build container.

Hit **Deploy**. In ~1 minute you'll have `https://yourgame.vercel.app`.

## 3. Publish the native binaries

The browser can't run the C++ game; players download it from GitHub Releases.

```bash
# on macOS
cmake -S game -B game/build -DCMAKE_BUILD_TYPE=Release
cmake --build game/build -j

cd game/build
zip -r fps_game-macos-arm64.zip fps_game fps_server assets

gh release create v0.1.0 fps_game-macos-arm64.zip --title "v0.1.0" --notes "First public build"
```

Repeat on a Windows machine (or use GitHub Actions — there's a CI workflow at
`.github/workflows/ci.yml` that already builds on all three OSes; just extend
it to upload artifacts to a Release).

## 4. Run a publicly-reachable `fps_server`

The matchmaker hands clients an IP:port that they then connect to over UDP.
For **LAN play** nothing extra is needed. For **internet play**, you need one
of:

### Option 4a — Cheap VPS (recommended)

Spin up a DigitalOcean / Hetzner / Linode droplet ($5-6/month), open UDP
port `7777`:

```bash
# on the VPS
git clone https://github.com/<you>/FPS-game
cd "FPS-game/game"
sudo apt install cmake libenet-dev libgl1-mesa-dev libx11-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libxi-dev libasound2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/fps_server --server=0.0.0.0:7777 --mode=team --map=range
```

Then set `GAME_SERVER_IP=<vps-ip>` and `GAME_SERVER_PORT=7777` on Railway.

### Option 4b — Home PC + port forwarding

Forward UDP 7777 on your router → your machine, run `fps_server` there, use
a DDNS hostname (e.g. DuckDNS) or your WAN IP as `GAME_SERVER_IP`.

### Option 4c — LAN only (testing)

Run `fps_server` on one machine on the LAN, set `GAME_SERVER_IP` to its LAN
IP. Players on the same WiFi can connect.

## 5. Test the full flow

1. Open `https://yourgame.vercel.app` in two browsers (or a browser + an
   incognito window).
2. Both click **Play** → **Team Deathmatch (5v5)** → pick a map → **Queue**.
3. With `TEAM_TEAM_SIZE=2`, a match forms as soon as both queue.
4. The site shows "Match found" and the exact command to paste into a local
   terminal where the binary lives.
5. Run the command on both machines. They connect to the `fps_server` over
   UDP and see each other move in real time.

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| Vercel build fails on `npm install` with Electron download error | Add env var `ELECTRON_SKIP_BINARY_DOWNLOAD=1` in Vercel project settings. |
| Browser shows "Matchmaking offline" | `VITE_MATCHMAKING_URL` wrong, or Railway service is sleeping on the free tier. Visit `/health` to wake it. |
| Match found but game can't connect | Check `GAME_SERVER_IP` on Railway. UDP 7777 needs to be reachable from clients. Try `nc -vu <ip> 7777` from another machine. |
| Clients see different maps | They probably used different `--map` flags. In team mode the first-queued player's map is sent to everyone; make sure the launch command includes `--map` from `match:found`. |
| CORS errors in browser console | The server already sends `Access-Control-Allow-Origin: *`. If you locked it down, add your Vercel domain to `cors()` in `server/src/index.ts`. |

## Going further (Option B)

To make the game **actually playable in Chrome** (no downloads), the native
C++ needs to be compiled to WebAssembly via Emscripten, and the enet UDP
transport swapped for WebSockets. That's 1-2 days of focused work; happy to
pick it up as a follow-up once Option A is live.
