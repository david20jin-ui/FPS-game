# @fps/launcher

React + Vite + Electron launcher. Queues for matches via Socket.io and spawns the C++ game binary on `match:found`.

```bash
npm run dev
```

Env: `VITE_MATCHMAKING_URL` (default `http://localhost:4000`), `FPS_GAME_BINARY` (overrides game binary location).
