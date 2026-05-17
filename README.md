# BobMan (Pacman Capstone in C++)

BobMan is a Pac-Man-inspired 2D game built with **C++17**, **SDL2**, and SDL extensions.
The current state includes the core game as well as a menu system, map management, difficulty settings, and an integrated map editor.

## Features (current state)

- **Start menu** with entries for:
  - Start game
  - Map selection
  - Map editor
  - Configuration
- **Difficulty based on monster count** (`Few`, `Medium`, `Many`).
- **Animated monsters** with direction-based sprites (`front`, `back`, `left`, `right`)
  and four animation frames per direction.
- **Multiple monster types** with different abilities:
  - `M` (purple): standard enemies
  - `N` (blue): gas clouds
  - `O` (red): fireballs with line-of-sight logic
  - `K` (green): additional standard enemy type
- **Teleporter tiles** (`1` to `5`) as paired portals.
- **Map library** from `data/maps/` with the map name in the first line.
- **In-game map editor**:
  - load an existing map
  - create a new map in a predefined size
  - validate and save the map
- **Audio effects** (optional via `AUDIO` in `src/definitions.h`).

## Project structure

- `src/main.cpp`: menu flow, game start, editor workflows.
- `src/game.*`: game rules, collisions, effects, teleporter handling.
- `src/map.*`: loading/saving/discovering maps, parsing, teleporter pairs.
- `src/renderer.*`: drawing the game, menus, HUD, editor, and effects.
- `src/events.*`: keyboard input and quit handling.
- `src/audio.*`: loading and playing sounds.
- `data/maps/`: map files (`.txt`, `.map`).

## Map format

A map file contains:
1. **Line 1:** display name of the map
2. **From line 2 onward:** layout

Important characters:
- `x` = wall
- `.` = path
- `G` = goodie
- `P` = Pacman start
- `M` = purple monster
- `N` = blue monster
- `O` = red monster
- `K` = green monster
- `1` to `5` = teleporter (each digit must appear exactly 0 or 2 times)

## Build

### Requirements

- CMake
- C++17 compiler (`g++` or `clang++`)
- SDL2 + SDL2_image + SDL2_ttf
- SDL2_mixer (for audio)

### Ubuntu / Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake \
  libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev
```

### Build and run

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
./BobMan
```

### Static Linux executable

To build a fully static Linux executable using Docker (includes all SDL2 libraries statically linked):

```bash
./build-linux-static.sh
```

The resulting static binary will be created at `build/BobMan-linux`.

Note: This requires Docker and builds all dependencies (SDL2, SDL2_image, SDL2_ttf, SDL2_mixer) from source.

### macOS release artifact

On macOS, `cmake --build --preset macos-app-release` now also creates a sanitized
distribution archive at `build/macos-app-release/dist/BobMan-macOS.zip`.
Use that ZIP for transferring the app to another Mac instead of the raw
`BobMan.app` from the build directory.

If your repository lives under `~/Documents` or another iCloud/File Provider
managed folder, Finder can attach metadata such as `com.apple.FinderInfo` to
the raw build-tree app bundle and break its code signature. To avoid that, the
macOS app presets build the real `.app` bundle in `/tmp/BobMan-app-<CONFIG>`
and leave a symlink at `build/<preset>/BobMan.app`. Use that symlink or the
generated launcher script `build/macos-app-release/Run BobMan.command`.

If you want Gatekeeper to open the app on another Mac without the
"Apple could not check it for malicious software" warning, you must sign it
with a `Developer ID Application` certificate and notarize it.

Example setup:

```bash
xcrun notarytool store-credentials bobman-notary \
  --apple-id "YOUR_APPLE_ID" \
  --team-id "YOUR_TEAM_ID" \
  --password "YOUR_APP_SPECIFIC_PASSWORD"

cmake --preset macos-app-release \
  -DBOBMAN_BUNDLE_IDENTIFIER=com.yourdomain.bobman \
  -DBOBMAN_CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
  -DBOBMAN_ENABLE_NOTARIZATION=ON \
  -DBOBMAN_NOTARY_KEYCHAIN_PROFILE=bobman-notary

cmake --build --preset macos-app-release
```

Without `BOBMAN_CODESIGN_IDENTITY`, the build falls back to ad-hoc signing for
local testing only.

## Controls

- **Menus:** arrow keys up/down, confirm with `Enter`, go back with `Esc`.
- **Game:** move Pacman with the arrow keys, quit with `Esc`.
- **Editor:**
  - move the cursor with the arrow keys
  - place/change tiles with the keys shown in the overlay
  - `Esc` opens the save/discard dialog

## Common problems

1. **SDL is not found**
   - Install the dependencies and run `cmake ..` again.

2. **No sound**
   - Enable `AUDIO` in `src/definitions.h` and rebuild.

3. **Map does not load**
   - Check that the first line is a name and that layout lines follow.
   - Teleporter digits must exist in pairs.
