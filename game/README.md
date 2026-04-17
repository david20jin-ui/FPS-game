# fps_game (C++ / Raylib)

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Produces `build/fps_game` and (if enet is installed) `build/fps_server`.

## Run

```bash
./build/fps_game --server=127.0.0.1:7777 --token=dev --name=Dev --mode=solo
```

## Controls

| Key        | Action                                    |
| ---------- | ----------------------------------------- |
| `WASD`     | Move                                      |
| `Shift`    | Walk (silent, smaller spread)             |
| `Ctrl/C`   | Crouch                                    |
| `Space`    | Jump                                      |
| Mouse      | Look                                      |
| `LMB`      | Fire                                      |
| `R`        | Reload                                    |
| `1-6`      | Vandal / Phantom / Spectre / Operator / Classic / Knife |
| Wheel      | Cycle weapons                             |
| `G`        | Throw smoke grenade                       |
| `F1`       | Debug: show navigation graph              |
| `Esc`      | Close window                              |
