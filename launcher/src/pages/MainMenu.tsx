import type { MatchMode } from "../types/protocol.js";

interface Props {
  name: string;
  onPlay: (mode: MatchMode) => void;
}

export function MainMenu({ name, onPlay }: Props): JSX.Element {
  return (
    <div className="page">
      <h1>Welcome back, {name}</h1>
      <p className="subtitle">Pick a mode to jump into a match.</p>
      <div className="grid-2" style={{ maxWidth: 720 }}>
        <button className="mode-card" onClick={() => onPlay("solo")}>
          <h3>Solo vs. Bots</h3>
          <p>Drop into a practice map against AI-controlled agents. Perfect for warmup and crosshair calibration.</p>
        </button>
        <button className="mode-card" onClick={() => onPlay("team")}>
          <h3>Team Deathmatch (5v5)</h3>
          <p>Queue into a live 10-player match on a shared dedicated server. Team-colored players, authoritative hits, kill feed.</p>
        </button>
      </div>
    </div>
  );
}
