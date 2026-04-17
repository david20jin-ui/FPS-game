import http from "node:http";
import cors from "cors";
import express from "express";
import { Server } from "socket.io";
import { Matchmaker } from "./matchmaker.js";
import type { QueueJoinPayload } from "./types.js";

const PORT = Number(process.env.PORT ?? 4000);

const app = express();
app.use(cors());
app.get("/health", (_req, res) => res.json({ ok: true }));

const httpServer = http.createServer(app);
const io = new Server(httpServer, { cors: { origin: "*" } });

const matchmaker = new Matchmaker(io, {
  soloTeamSize: Number(process.env.SOLO_TEAM_SIZE ?? 1),
  teamTeamSize: Number(process.env.TEAM_TEAM_SIZE ?? 2),
});

io.on("connection", (socket) => {
  console.log(`[socket] connected ${socket.id}`);

  socket.on("queue:join", (payload: QueueJoinPayload) => {
    if (!payload?.playerId || !payload.name || !payload.mode) {
      socket.emit("error", { code: "BAD_PAYLOAD", message: "queue:join requires playerId, name and mode" });
      return;
    }
    matchmaker.join(socket, payload);
  });

  socket.on("queue:leave", () => matchmaker.leave(socket.id));

  socket.on("disconnect", () => {
    matchmaker.leave(socket.id);
    console.log(`[socket] disconnected ${socket.id}`);
  });
});

httpServer.listen(PORT, () => {
  console.log(`[server] matchmaking listening on http://localhost:${PORT}`);
});

const shutdown = () => {
  matchmaker.shutdown();
  httpServer.close(() => process.exit(0));
};
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
