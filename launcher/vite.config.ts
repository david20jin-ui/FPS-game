import { defineConfig, type PluginOption } from "vite";
import react from "@vitejs/plugin-react";

// When building for the web (Vercel, GitHub Pages, etc.) set
// `FPS_BUILD_TARGET=web` to skip the Electron bundle entirely.
//
// We load the electron plugins via dynamic `import()` because their top-level
// side effects require `esbuild` at resolution time; on Vercel with npm
// workspace hoisting that resolution sometimes fails even though esbuild is
// installed elsewhere. Keeping them lazy means the web build never touches
// them.
const isWebBuild = process.env.FPS_BUILD_TARGET === "web";

export default defineConfig(async () => {
  const plugins: PluginOption[] = [react()];

  if (!isWebBuild) {
    const [{ default: electron }, { default: renderer }] = await Promise.all([
      import("vite-plugin-electron"),
      import("vite-plugin-electron-renderer"),
    ]);
    plugins.push(
      electron([
        {
          entry: "electron/main.ts",
          vite: {
            build: {
              outDir: "dist-electron",
              rollupOptions: { external: ["electron"] },
            },
          },
        },
        {
          entry: "electron/preload.ts",
          onstart(options) {
            options.reload();
          },
          vite: {
            build: {
              outDir: "dist-electron",
              rollupOptions: { external: ["electron"] },
            },
          },
        },
      ]),
      renderer()
    );
  }

  return {
    plugins,
    server: { port: 5173 },
  };
});
