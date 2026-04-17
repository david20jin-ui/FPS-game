export {};

declare global {
  interface FpsLaunchArgs {
    ip: string;
    port: number;
    token: string;
    name: string;
    mode: string;
    fov?: number;
    sensitivity?: number;
    difficulty?: string;
    mapId?: string;
  }

  interface FpsLaunchResult {
    ok: boolean;
    pid?: number;
    error?: string;
  }

  interface Window {
    fpsApi?: {
      launchGame: (args: FpsLaunchArgs) => Promise<FpsLaunchResult>;
    };
  }
}
