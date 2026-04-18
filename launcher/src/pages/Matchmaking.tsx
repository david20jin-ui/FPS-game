import { useEffect, useState } from "react";
import type { Socket } from "socket.io-client";
import {
  DIFFICULTIES,
  MAPS,
  type MatchFoundPayload,
  type MatchMode,
  type QueueUpdatePayload,
} from "../types/protocol.js";

interface Props {
  socket: Socket | null;
  connected: boolean;
  profile: {
    name: string;
    playerId: string;
    fov?: number;
    sensitivity?: number;
    difficulty?: string;
    mapId?: string;
  };
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
      mapId: profile.mapId,
    });
    setStatus({ kind: "queued" });

    const onUpdate = (update: QueueUpdatePayload) =>
      setStatus((s) => (s.kind === "queued" ? { kind: "queued", update } : s));
    const onFound = async (match: MatchFoundPayload) => {
      setStatus({ kind: "found", match });
      if (window.fpsApi) {
        // Team matches use the map the server picked (from the first queued
        // player). Solo matches use the player's own picker.
        const launchMapId = match.mapId ?? profile.mapId;
        const launch = await window.fpsApi.launchGame({
          ip: match.ip,
          port: match.port,
          token: match.token,
          name: profile.name,
          mode: match.mode,
          fov: profile.fov,
          sensitivity: profile.sensitivity,
          difficulty: profile.difficulty,
          mapId: launchMapId,
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
    const launchCommand =
      `./fps_game --mode=${match.mode} --server=${match.ip}:${match.port}` +
      ` --name=${profile.name}${match.mapId ? ` --map=${match.mapId}` : ""}`;

    const copyCmd = () => {
      navigator.clipboard?.writeText(launchCommand).catch(() => {});
    };

    let launchBanner: JSX.Element;
    if (isElectron) {
      launchBanner = launch
        ? (launch.ok
            ? <span>Game launched (pid {launch.pid}).</span>
            : <span>{launch.error}</span>)
        : <span>Launching game&hellip;</span>;
    } else {
      launchBanner = (
        <span>
          You're on the web launcher. Open your local game with the command
          below (or click <strong>Install</strong> in the sidebar if you don't
          have it yet).
        </span>
      );
    }

    // In browser mode (no Electron), the banner is informational rather than
    // an error — apply the "ok" style so it reads as green instead of red.
    const bannerClass = launch?.ok || !isElectron ? "banner ok" : "banner";
    return (
      <div className="page matchmaking">
        <h1>Match found</h1>
        <div className={bannerClass}>{launchBanner}</div>
        <div className="card match-info" style={{ minWidth: 420 }}>
          <p>Match <strong>{match.matchId.slice(0, 8)}</strong></p>
          <p>Server <strong>{match.ip}:{match.port}</strong></p>
          <p>Mode <strong>{match.mode}</strong></p>
        </div>
        {!isElectron && (
          <div className="card" style={{ minWidth: 420, maxWidth: 560 }}>
            <div style={{ fontSize: 12, color: "var(--text-muted)", marginBottom: 6 }}>
              LAUNCH COMMAND
            </div>
            <code style={{
              display: "block",
              padding: "10px 12px",
              background: "var(--bg-1)",
              borderRadius: 6,
              fontSize: 12,
              wordBreak: "break-all",
            }}>{launchCommand}</code>
            <button className="cta secondary" style={{ marginTop: 10 }}
                    onClick={copyCmd}>
              Copy to clipboard
            </button>
          </div>
        )}
        <button className="cta secondary" onClick={onCancel}>Return to menu</button>
      </div>
    );
  }

  const update = status.kind === "queued" ? status.update : undefined;

  const diffInfo = DIFFICULTIES.find((d) => d.id === profile.difficulty);
  const mapInfo  = MAPS.find((m) => m.id === profile.mapId);

  return (
    <div className="page matchmaking">
      <div className="spinner" />
      <h1>Searching for a match</h1>
      <div className="match-info">
        <p>Mode: <strong>{mode}</strong> · Elapsed: <strong>{elapsed}s</strong></p>
        {mode === "solo" && (diffInfo || mapInfo) && (
          <p>
            {mapInfo && <>Map <strong>{mapInfo.name}</strong></>}
            {mapInfo && diffInfo && " · "}
            {diffInfo && <>Difficulty <strong>{diffInfo.name}</strong></>}
          </p>
        )}
        {update && (
          <p>Queue position <strong>{update.position}</strong> of <strong>{update.playersInQueue}</strong> (need {update.requiredPlayers})</p>
        )}
      </div>
      <button className="cta secondary" onClick={onCancel}>Cancel</button>
    </div>
  );
}
