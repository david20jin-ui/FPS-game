import { useEffect, useState } from "react";

type OS = "mac" | "windows" | "linux" | "unknown";

function detectOS(): OS {
  const ua = navigator.userAgent.toLowerCase();
  if (ua.includes("mac")) return "mac";
  if (ua.includes("win")) return "windows";
  if (ua.includes("linux")) return "linux";
  return "unknown";
}

// The launcher defaults to the repo's GitHub Releases. Override at deploy
// time via the VITE_DOWNLOADS_BASE env var to point at your own bucket.
const DOWNLOADS_BASE =
  import.meta.env.VITE_DOWNLOADS_BASE ??
  "https://github.com/david20jin-ui/FPS-game/releases/latest/download";

const downloads: Record<OS, { label: string; file: string } | null> = {
  mac:     { label: "macOS (Apple Silicon)", file: "fps_game-macos-arm64.zip" },
  windows: { label: "Windows (x64)",          file: "fps_game-windows-x64.zip" },
  linux:   { label: "Linux (x64)",            file: "fps_game-linux-x64.tar.gz" },
  unknown: null,
};

interface Props {
  onBack: () => void;
}

export function Downloads({ onBack }: Props): JSX.Element {
  const [os, setOs] = useState<OS>("unknown");
  useEffect(() => setOs(detectOS()), []);

  const pick = downloads[os];

  return (
    <div className="page">
      <h1>Install the game</h1>
      <p className="subtitle">
        The browser runs matchmaking and chat. The actual 3D game is a native
        app that talks to this site over the web.
      </p>

      <div className="card" style={{ maxWidth: 640, marginBottom: 24 }}>
        <h3 style={{ marginTop: 0 }}>1. Download</h3>
        {pick ? (
          <p>
            We detected <strong>{pick.label}</strong>.
            <br />
            <a className="cta" style={{ display: "inline-block", marginTop: 12 }}
               href={`${DOWNLOADS_BASE}/${pick.file}`}>
              Download {pick.label}
            </a>
          </p>
        ) : (
          <p>Pick your platform:</p>
        )}
        <div style={{ display: "flex", gap: 8, flexWrap: "wrap", marginTop: 12 }}>
          {(Object.keys(downloads) as OS[])
            .filter((k) => downloads[k])
            .map((k) => {
              const d = downloads[k]!;
              return (
                <a key={k} className="cta secondary" href={`${DOWNLOADS_BASE}/${d.file}`}>
                  {d.label}
                </a>
              );
            })}
        </div>
      </div>

      <div className="card" style={{ maxWidth: 640, marginBottom: 24 }}>
        <h3 style={{ marginTop: 0 }}>2. Run it</h3>
        <p style={{ color: "var(--text-muted)", fontSize: 13 }}>
          Unzip and double-click <code>fps_game</code>. On macOS the first
          launch may need <strong>System Settings → Privacy &amp; Security → Open
          Anyway</strong> since the binary isn't notarized yet.
        </p>
      </div>

      <div className="card" style={{ maxWidth: 640, marginBottom: 24 }}>
        <h3 style={{ marginTop: 0 }}>3. Come back here to queue</h3>
        <p style={{ color: "var(--text-muted)", fontSize: 13 }}>
          With the game running, hit <strong>Play</strong> above. When a match
          is found the site hands the server address to the game.
        </p>
      </div>

      <button className="cta secondary" onClick={onBack}>Back to menu</button>
    </div>
  );
}
