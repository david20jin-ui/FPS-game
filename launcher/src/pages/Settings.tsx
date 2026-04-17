interface Profile {
  name: string;
  playerId: string;
  sensitivity: number;
  fov: number;
}

interface Props {
  profile: Profile;
  onChange: (patch: Partial<Profile>) => void;
}

export function Settings({ profile, onChange }: Props): JSX.Element {
  return (
    <div className="page">
      <h1>Settings</h1>
      <p className="subtitle">Changes are saved locally and passed to the game on launch.</p>
      <div className="card" style={{ maxWidth: 480 }}>
        <div className="form-row">
          <label htmlFor="name">Display name</label>
          <input id="name" value={profile.name} onChange={(e) => onChange({ name: e.target.value })} />
        </div>
        <div className="form-row">
          <label htmlFor="sens">Mouse sensitivity ({profile.sensitivity.toFixed(2)})</label>
          <input id="sens" type="range" min={0.05} max={2} step={0.05}
            value={profile.sensitivity} onChange={(e) => onChange({ sensitivity: Number(e.target.value) })} />
        </div>
        <div className="form-row">
          <label htmlFor="fov">Field of view ({profile.fov}°)</label>
          <input id="fov" type="range" min={70} max={110} step={1}
            value={profile.fov} onChange={(e) => onChange({ fov: Number(e.target.value) })} />
        </div>
        <div className="form-row">
          <label>Player ID</label>
          <input value={profile.playerId} readOnly />
        </div>
      </div>
    </div>
  );
}
