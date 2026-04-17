import { useEffect, useState } from "react";
import type { Socket } from "socket.io-client";
import type {
  MatchFoundPayload,
  MatchMode,
  QueueUpdatePayload,
} from "../types/protocol.js";

interface Props {
  socket: Socket | null;
  connected: boolean;
  profile: { name: string; playerId: string; fov?: number; sensitivity?: number };
  mode: MatchMode;
  isElectron: boolean;
  onCancel: () => void;
}

type Status =
  | { kind: "idle" }
  | { kind: "queued"; update?: QueueUpdatePayload }
  | { kind: "found"; match: MatchFoundPayload; launch?: FpsLaunchResult }
  | { kind: "error"; message: string };

export function Matchmaking({
  socket, connected, profile, mode, isElectron, onCancel,
}: Props): JSX.Element {
  const [status, setStatus] = useState<Status>({ kind: "idle" });
  const [elapsed, setElapsed] = useState(0);

  useEffect(() => {
    if (!socket || !connected) return;

    socket.emit("queue:join", {
      playerId: profile.playerId,
      name: profile.name,
      mode,
    });
    setStatus({ kind: "queued" });

    const onUpdate = (update: QueueUpdatePayload) =>
      setStatus((s) => (s.kind === "queued" ? { kind: "queued", update } : s));
    const onFound = async (match: MatchFoundPayload) => {
      setStatus({ kind: "found", match });
      if (window.fpsApi) {
        const launch = await window.fpsApi.launchGame({
          ip: match.ip,
          port: match.port,
          token: match.token,
          name: profile.name,
          mode: match.mode,
          fov: profile.fov,
          sensitivity: profile.sensitivity,
        });
        setStatus({ kind: "found", match, launch });
      }
    };
    const onError = (err: { code: string; message: string }) =>
      setStatus({ kind: "error", message: `${err.code}: ${err.message}` });

    socket.on("queue:update", onUpdate);
    socket.on("match:found", onFound);
    socket.on("error", onError);

    return () => {
      socket.off("queue:update", onUpdate);
      socket.off("match:found", onFound);
      socket.off("error", onError);
      socket.emit("queue:leave");
    };
  }, [socket, connected, profile, mode]);

  useEffect(() => {
    if (status.kind !== "queued") return;
    const start = Date.now();
    setElapsed(0);
    const t = setInterval(() => setElapsed(Math.floor((Date.now() - start) / 1000)), 250);
    return () => clearInterval(t);
  }, [status.kind]);

  if (!connected) {
    return (
      <div className="page matchmaking">
        <div className="banner">
          Matchmaking server is offline. Start it with <code>npm run dev:server</code>.
        </div>
        <button className="cta secondary" onClick={onCancel}>Back</button>
      </div>
    );
  }

  if (status.kind === "error") {
    return (
      <div className="page matchmaking">
        <div className="banner">{status.message}</div>
        <button className="cta secondary" onClick={onCancel}>Back</button>
      </div>
    );
  }

  if (status.kind === "found") {
    const { match, launch } = status;
    const launchBanner = !isElectron
      ? "Run the launcher via Electron to auto-start the game. In browser mode the handoff is only displayed below."
      : launch
        ? launch.ok ? `Game launched (pid ${launch.pid}).` : launch.error
        : "Launching game...";

    return (
      <div className="page matchmaking">
        <h1>Match found</h1>
        <div className={`banner ${launch?.ok ? "ok" : ""}`}>{launchBanner}</div>
        <div className="card match-info" style={{ minWidth: 420 }}>
          <p>Match <strong>{match.matchId.slice(0, 8)}</strong></p>
          <p>Server <strong>{match.ip}:{match.port}</strong></p>
          <p>Mode <strong>{match.mode}</strong></p>
        </div>
        <button className="cta secondary" onClick={onCancel}>Return to menu</button>
      </div>
    );
  }

  const update = status.kind === "queued" ? status.update : undefined;

  return (
    <div className="page matchmaking">
      <div className="spinner" />
      <h1>Searching for a match</h1>
      <div className="match-info">
        <p>Mode: <strong>{mode}</strong> · Elapsed: <strong>{elapsed}s</strong></p>
        {update && (
          <p>Queue position <strong>{update.position}</strong> of <strong>{update.playersInQueue}</strong> (need {update.requiredPlayers})</p>
        )}
      </div>
      <button className="cta secondary" onClick={onCancel}>Cancel</button>
    </div>
  );
}
