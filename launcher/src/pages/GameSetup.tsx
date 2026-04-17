import type { Difficulty, MapId, MatchMode } from "../types/protocol.js";
import { DIFFICULTIES, MAPS } from "../types/protocol.js";

interface Props {
  mode: MatchMode;
  difficulty: Difficulty;
  mapId: MapId;
  onDifficulty: (d: Difficulty) => void;
  onMap: (m: MapId) => void;
  onStart: () => void;
  onBack: () => void;
}

export function GameSetup({
  mode,
  difficulty,
  mapId,
  onDifficulty,
  onMap,
  onStart,
  onBack,
}: Props): JSX.Element {
  const isTeam = mode === "team";
  return (
    <div className="page">
      <h1>{isTeam ? "Team match setup" : "Match setup"}</h1>
      <p className="subtitle">
        {isTeam
          ? "Pick a map, then queue. The first player's map is the one used for the match."
          : "Pick a difficulty and a map, then start the search."}
      </p>

      {!isTeam && (
        <section style={{ marginBottom: 32 }}>
          <h3 style={{ margin: "0 0 12px", color: "var(--text-muted)", fontSize: 14, letterSpacing: 2, textTransform: "uppercase" }}>
            Difficulty
          </h3>
          <div className="grid-3">
            {DIFFICULTIES.map((d) => {
              const selected = d.id === difficulty;
              return (
                <button
                  key={d.id}
                  className={`option-card ${selected ? "selected" : ""}`}
                  onClick={() => onDifficulty(d.id)}
                  style={{ borderColor: selected ? d.accent : undefined }}
                >
                  <div className="option-accent" style={{ background: d.accent }} />
                  <h4>{d.name}</h4>
                  <p>{d.description}</p>
                </button>
              );
            })}
          </div>
        </section>
      )}

      <section style={{ marginBottom: 32 }}>
        <h3 style={{ margin: "0 0 12px", color: "var(--text-muted)", fontSize: 14, letterSpacing: 2, textTransform: "uppercase" }}>
          Map
        </h3>
        <div className="grid-3">
          {MAPS.map((m) => {
            const selected = m.id === mapId;
            return (
              <button
                key={m.id}
                className={`option-card ${selected ? "selected" : ""}`}
                onClick={() => onMap(m.id)}
              >
                <div className="map-preview" data-map={m.id} />
                <h4>{m.name}</h4>
                <p>{m.description}</p>
              </button>
            );
          })}
        </div>
      </section>

      {isTeam && (
        <div className="banner ok" style={{ marginBottom: 16 }}>
          Team mode queues 10 players (5v5) and spins up a dedicated server.
          Bots are disabled; you'll fight other real players.
        </div>
      )}

      <div style={{ display: "flex", gap: 12 }}>
        <button className="cta secondary" onClick={onBack}>Back</button>
        <button className="cta" onClick={onStart}>
          {isTeam ? "Queue for 5v5" : "Start match"}
        </button>
      </div>
    </div>
  );
}
