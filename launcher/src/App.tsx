import { useMemo, useState } from "react";
import { useSocket } from "./hooks/useSocket.js";
import { MainMenu } from "./pages/MainMenu.js";
import { Matchmaking } from "./pages/Matchmaking.js";
import { Settings } from "./pages/Settings.js";
import type { MatchMode } from "./types/protocol.js";

export type Page = "home" | "matchmaking" | "settings";

function loadProfile(): { name: string; playerId: string; sensitivity: number; fov: number } {
  const raw = localStorage.getItem("fps:profile");
  if (raw) {
    try { return JSON.parse(raw); } catch { /* fall through */ }
  }
  const profile = {
    name: `Agent-${Math.floor(Math.random() * 9999)}`,
    playerId: crypto.randomUUID(),
    sensitivity: 0.4,
    fov: 90,
  };
  localStorage.setItem("fps:profile", JSON.stringify(profile));
  return profile;
}

export function App(): JSX.Element {
  const [page, setPage] = useState<Page>("home");
  const [queueMode, setQueueMode] = useState<MatchMode>("solo");
  const [profile, setProfile] = useState(() => loadProfile());
  const { socket, connected } = useSocket();

  const updateProfile = (patch: Partial<typeof profile>): void => {
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
          <button className={`nav-item ${page === "settings" ? "active" : ""}`} onClick={() => setPage("settings")}>Settings</button>
        </nav>
        <div className="status-pill">
          <span className={`status-dot ${connected ? "ok" : ""}`} />
          {connected ? "Matchmaking online" : "Matchmaking offline"}
        </div>
      </aside>
      <main className="main">
        {page === "home" && (
          <MainMenu name={profile.name} onPlay={(mode) => { setQueueMode(mode); setPage("matchmaking"); }} />
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
      </main>
    </div>
  );
}
