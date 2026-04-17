import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import electron from "vite-plugin-electron";
import renderer from "vite-plugin-electron-renderer";

// When building for the web (Vercel, GitHub Pages, etc.), set
// `FPS_BUILD_TARGET=web` to skip the Electron bundle entirely. The same React
// code runs — it just detects `window.fpsApi` is absent at runtime and falls
// back to a "download to play" flow.
const isWebBuild = process.env.FPS_BUILD_TARGET === "web";

export default defineConfig({
  plugins: [
    react(),
    ...(isWebBuild
      ? []
      : [
          electron([
            {
              entry: "electron/main.ts",
              vite: {
                build: { outDir: "dist-electron", rollupOptions: { external: ["electron"] } },
              },
            },
            {
              entry: "electron/preload.ts",
              onstart(options) { options.reload(); },
              vite: {
                build: { outDir: "dist-electron", rollupOptions: { external: ["electron"] } },
              },
            },
          ]),
          renderer(),
        ]),
  ],
  server: { port: 5173 },
});
