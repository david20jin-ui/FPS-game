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
          <h3>Unrated (Team)</h3>
          <p>Standard 5v5-style plant/defuse. (MVP queues 2 players to form a match.)</p>
        </button>
      </div>
    </div>
  );
}
