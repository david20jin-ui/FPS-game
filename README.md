# FPS Game – Valorant-Inspired Shooter

A Valorant-inspired first-person shooter built as a three-module monorepo:

- **[launcher/](launcher/)** – React + Vite + Electron matchmaking UI.
- **[server/](server/)** – Node.js + Socket.io matchmaking service.
- **[game/](game/)** – C++20 + Raylib game engine (`fps_game` client + `fps_server` dedicated server).
- **[shared/](shared/protocol.md)** – wire-protocol contract.

## Quick start

```bash
# macOS prerequisites
brew install cmake raylib enet

npm install
npm run build:game

npm run dev:server    # Terminal 1 – matchmaking server
npm run dev:launcher  # Terminal 2 – Electron launcher

# Or run the game directly:
./game/build/fps_game --name=Dev
```

## Controls

`WASD` move, `Shift` walk, `Ctrl/C` crouch, `Space` jump, `LMB` fire, `R` reload, `1-6` weapons, `G` smoke, mouse wheel cycles weapons, `F1` debug nav graph, `Esc` quit.
