import { contextBridge, ipcRenderer } from "electron";

export interface LaunchGameArgs {
  ip: string;
  port: number;
  token: string;
  name: string;
  mode: string;
  fov?: number;
  sensitivity?: number;
}

export interface LaunchGameResult {
  ok: boolean;
  pid?: number;
  error?: string;
}

contextBridge.exposeInMainWorld("fpsApi", {
  launchGame: (args: LaunchGameArgs): Promise<LaunchGameResult> =>
    ipcRenderer.invoke("fps:launchGame", args),
});
