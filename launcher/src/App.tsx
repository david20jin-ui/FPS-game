import { useMemo, useState } from "react";
import { useSocket } from "./hooks/useSocket.js";
import { Downloads } from "./pages/Downloads.js";
import { GameSetup } from "./pages/GameSetup.js";
import { MainMenu } from "./pages/MainMenu.js";
import { Matchmaking } from "./pages/Matchmaking.js";
import { Settings } from "./pages/Settings.js";
import type { Difficulty, MapId, MatchMode } from "./types/protocol.js";

export type Page = "home" | "setup" | "matchmaking" | "settings" | "downloads";

interface Profile {
  name: string;
  playerId: string;
  sensitivity: number;
  fov: number;
  difficulty: Difficulty;
  mapId: MapId;
}

function loadProfile(): Profile {
  const raw = localStorage.getItem("fps:profile");
  if (raw) {
    try {
      const parsed = JSON.parse(raw);
      // Backfill new fields if saved from an older version.
      return {
        name: parsed.name ?? `Agent-${Math.floor(Math.random() * 9999)}`,
        playerId: parsed.playerId ?? crypto.randomUUID(),
        sensitivity: parsed.sensitivity ?? 0.4,
        fov: parsed.fov ?? 90,
        difficulty: (parsed.difficulty as Difficulty) ?? "medium",
        mapId: (parsed.mapId as MapId) ?? "range",
      };
    } catch {
      /* fall through */
    }
  }
  const profile: Profile = {
    name: `Agent-${Math.floor(Math.random() * 9999)}`,
    playerId: crypto.randomUUID(),
    sensitivity: 0.4,
    fov: 90,
    difficulty: "medium",
    mapId: "range",
  };
  localStorage.setItem("fps:profile", JSON.stringify(profile));
  return profile;
}

export function App(): JSX.Element {
  const [page, setPage] = useState<Page>("home");
  const [queueMode, setQueueMode] = useState<MatchMode>("solo");
  const [profile, setProfile] = useState<Profile>(() => loadProfile());
  const { socket, connected } = useSocket();

  const updateProfile = (patch: Partial<Profile>): void => {
    const next = { ...profile, ...patch };
    setProfile(next);
    localStorage.setItem("fps:profile", JSON.stringify(next));
  };

  const isElectron = useMemo(() => typeof window.fpsApi !== "undefined", []);

  return (
    <div className="app">
      <aside className="sidebar">
        <div className="brand">FPS//</div>
        <nav className="nav">
          <button className={`nav-item ${page === "home" ? "active" : ""}`} onClick={() => setPage("home")}>Play</button>
          {!isElectron && (
            <button className={`nav-item ${page === "downloads" ? "active" : ""}`} onClick={() => setPage("downloads")}>Install</button>
          )}
          <button className={`nav-item ${page === "settings" ? "active" : ""}`} onClick={() => setPage("settings")}>Settings</button>
        </nav>
        <div className="status-pill">
          <span className={`status-dot ${connected ? "ok" : ""}`} />
          {connected ? "Matchmaking online" : "Matchmaking offline"}
        </div>
      </aside>
      <main className="main">
        {page === "home" && (
          <MainMenu
            name={profile.name}
            onPlay={(mode) => {
              setQueueMode(mode);
              // Both modes pop the setup page so the player can pick a map
              // (team mode hides the difficulty picker since bots are off).
              setPage("setup");
            }}
          />
        )}
        {page === "setup" && (
          <GameSetup
            mode={queueMode}
            difficulty={profile.difficulty}
            mapId={profile.mapId}
            onDifficulty={(d) => updateProfile({ difficulty: d })}
            onMap={(m) => updateProfile({ mapId: m })}
            onStart={() => setPage("matchmaking")}
            onBack={() => setPage("home")}
          />
        )}
        {page === "matchmaking" && (
          <Matchmaking
            socket={socket}
            connected={connected}
            profile={profile}
            mode={queueMode}
            isElectron={isElectron}
            onCancel={() => setPage("home")}
          />
        )}
        {page === "settings" && (
          <Settings profile={profile} onChange={updateProfile} />
        )}
        {page === "downloads" && (
          <Downloads onBack={() => setPage("home")} />
        )}
      </main>
    </div>
  );
}
