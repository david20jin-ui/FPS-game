// Monorepo entry shim so that "node index.js" or "node ." from the repo
// root resolves to the actual server. Railway / Nixpacks auto-detection
// falls through to this when no explicit start command is set.
require("./server/dist/index.js");
