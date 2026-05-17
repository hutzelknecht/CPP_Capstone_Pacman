# BobMan Migration Plan: C++/SDL2 → Python/PySDL2

## Overview

This document outlines the plan to port BobMan from C++ with SDL2 to Python using PySDL2.

## Technology Choice: PySDL2

**Rationale:**
- PySDL2 provides **direct 1:1 Python bindings** to SDL2, matching the existing C++ SDL2 API
- Minimal conceptual translation: SDL2 function calls remain identical
- Full access to all SDL2, SDL2_image, SDL2_ttf, SDL2_mixer features
- Preserves the existing game architecture and logic flow

**Alternative considered:** Pygame was rejected because it abstracts SDL2, requiring API translation and hiding features we use (e.g., advanced renderer features, custom event handling).

---

## Installation

```bash
# Create virtual environment
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate

# Install dependencies
pip install PySDL2 PySDL2-dll
```

Note: `PySDL2-dll` provides the SDL2 binaries on platforms that need them.

---

## Architecture Mapping

### Directory Structure

```
C++ (current)                    Python (target)
─────────────────────────────────────────────────
src/                              bobman/
├── main.cpp           →         main.py       (entry point)
├── app.cpp/h          →         app.py        (menu, app loop)
├── game.cpp/h         →         game.py       (Game class)
├── renderer.cpp/h     →         renderer.py   (SDL rendering)
├── events.cpp/h       →         events.py     (input handling)
├── audio.cpp/h        →         audio.py      (sound/music)
├── map.cpp/h          →         map.py        (map loading)
├── pacman.cpp/h       →         pacman.py     (player logic)
├── monster.cpp/h      →         monster.py    (enemy logic)
├── goodie.cpp/h       →         goodie.py     (pickups)
└── definitions.h      →         constants.py (config values)

data/                           data/ (unchanged - maps, sprites, audio)
```

### Class/Struct Mapping

| C++ Type | Python Type | Notes |
|----------|-------------|-------|
| `struct MapCoord` | `@dataclass` `MapCoord` | or `NamedTuple` |
| `enum class Directions` | `enum.Enum` | `class Directions(Enum): Up=0, Down=1, ...` |
| `class Game` | `class Game` | Direct port, SDL types replaced with PySDL2 equivalents |
| `class Pacman` | `class Pacman` | |
| `class Monster` | `class Monster` | |
| `SDL_Rect` | `sdl2.SDL_Rect` | Or create Python wrapper class |
| `SDL_Texture*` | `sdl2.SDL_Texture` | Python object, no pointer |
| `Mix_Chunk*` | `sdl2.mixer.Mix_Chunk` | |
| `Mix_Music*` | `sdl2.mixer.Mix_Music` | |

### SDL2 API Mapping

| C++ SDL2 | Python PySDL2 |
|----------|--------------|
| `SDL_Init()` | `sdl2.SDL_Init()` |
| `SDL_CreateRenderer()` | `sdl2.SDL_CreateRenderer()` |
| `SDL_CreateWindow()` | `sdl2.SDL_CreateWindow()` |
| `SDL_LoadBMP()` | `sdl2.SDL_LoadBMP()` |
| `SDL_RenderClear()` | `sdl2.SDL_RenderClear()` |
| `SDL_RenderCopy()` | `sdl2.SDL_RenderCopy()` |
| `SDL_RenderPresent()` | `sdl2.SDL_RenderPresent()` |
| `SDL_PollEvent()` | `sdl2.SDL_PollEvent()` |
| `SDL_PumpEvents()` | `sdl2.SDL_PumpEvents()` |
| `Mix_OpenAudio()` | `sdl2.mixer.Mix_OpenAudio()` |
| `Mix_LoadWAV()` | `sdl2.mixer.Mix_LoadWAV()` |
| `Mix_LoadMUS()` | `sdl2.mixer.Mix_LoadMUS()` |
| `Mix_PlayChannel()` | `sdl2.mixer.Mix_PlayChannel()` |
| `Mix_PlayMusic()` | `sdl2.mixer.Mix_PlayMusic()` |

---

## Migration Phases

### Phase 1: Project Skeleton (1 day)

1. Create `bobman/` directory structure
2. Create empty stub files for each module
3. Set up `requirements.txt`
4. Create basic `main.py` with SDL2 initialization
5. Verify PySDL2 works with a simple window

**Deliverable:** Project runs, displays a blank window with the game title.

### Phase 2: Resource Loading (1-2 days)

1. Port `Renderer` class for texture loading
2. Load sprites from `data/` directory
3. Implement `Map` class for map parsing
4. Display static map on screen

**Deliverable:** Game shows the map background and static elements.

### Phase 3: Core Game Loop (2-3 days)

1. Port `Events` class for input handling
2. Port `Pacman` class with movement logic
3. Implement collision detection
4. Basic game loop: input → update → render

**Deliverable:** Pacman moves around the map with arrow keys.

### Phase 4: Entities (2 days)

1. Port `Monster` class with AI logic
2. Port `Goodie` class
3. Implement monster movement threads (using Python `threading`)
4. Implement pickup collection

**Deliverable:** Monsters move, goodies can be collected.

### Phase 5: Audio (1 day)

1. Port `Audio` class using `sdl2.mixer`
2. Load all sound effects and music
3. Trigger sounds on events (coin, death, etc.)

**Deliverable:** Game has sound effects and background music.

### Phase 6: Special Features (2-3 days)

1. Teleporters
2. Nuclear bombs, dynamite, etc.
3. Disco ball easter egg
4. Alien spawning
5. All power-ups

**Deliverable:** Full feature parity with C++ version.

### Phase 7: Menu System (2 days)

1. Port menu rendering
2. Port menu navigation
3. Settings screen
4. Map selection

**Deliverable:** Full menu system working.

### Phase 8: Map Editor (Optional, 3-4 days)

1. Port editor UI
2. Map saving/loading
3. Validation

### Phase 9: Docker Cross-Platform Build System (2 days)

1. Create `docker/` directory with Dockerfiles for each platform
2. Set up Docker Buildx for multi-platform builds
3. Create `build-all.sh` script
4. Test builds on CI/CD (GitHub Actions)
5. Document build process for all platforms

**Deliverable:** Docker builds produce working executables for Linux, macOS, and Windows.

---

## Key Differences & Considerations

### Memory Management

**C++:** Manual `new`/`delete`, raw pointers
**Python:** Garbage collected, use PySDL2 objects directly

```cpp
// C++
SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
SDL_FreeSurface(surface);
// Later: SDL_DestroyTexture(texture);
```

```python
# Python
texture = sdl2.SDL_CreateTextureFromSurface(renderer, surface)
sdl2.SDL_FreeSurface(surface)
# No explicit destroy needed - garbage collected
# But recommended to call sdl2.SDL_DestroyTexture(texture) for cleanup
```

### Threading

**C++:** `std::thread` for monster/pacman movement
**Python:** Use `threading.Thread` but be aware of the **Global Interpreter Lock (GIL)**

- CPU-bound threads won't run in parallel due to GIL
- SDL2 operations release the GIL automatically in PySDL2
- For true parallelism, consider `multiprocessing` but that adds complexity
- **Recommendation:** Keep the same threading model, but profile for GIL issues

### Error Handling

**C++:** Check for `nullptr`, use assertions
**Python:** Use exceptions, check for `None`

```cpp
// C++
if (texture == nullptr) {
    std::cerr << "Failed to load: " << SDL_GetError() << std::endl;
}
```

```python
# Python
if texture is None:
    raise RuntimeError(f"Failed to load: {sdl2.SDL_GetError()}")
```

### Performance

Python + PySDL2 will be slower than C++. Mitigation strategies:
- Use sprite sheets instead of individual textures
- Minimize object creation in hot loops
- Use `__slots__` in classes with many instances (monsters, particles)
- Profile with `cProfile` and optimize hotspots

---

## Testing Strategy

1. **Unit Tests:** Create tests for collision detection, pathfinding, map parsing
2. **Integration Tests:** Test subsystems together (rendering + input, etc.)
3. **Visual Regression:** Compare screenshots of C++ and Python versions
4. **Performance Benchmarks:** Measure FPS and memory usage

---

## Cross-Platform Build System (Docker)

To ensure consistent builds across Linux, macOS, and Windows, we use Docker with multi-platform support.

### Docker Setup

```bash
# Build for all platforms (requires Docker Buildx)
docker buildx create --use
```

### Directory Structure

```
docker/
├── Dockerfile.linux      # Ubuntu-based build
├── Dockerfile.macos     # macOS cross-compile (using osxcross)
├── Dockerfile.windows   # Windows cross-compile (using mingw-w64)
└── build-all.sh          # Script to build for all platforms

build/
├── linux/               # Linux build artifacts
├── macos/               # macOS build artifacts
└── windows/             # Windows build artifacts
```

### Dockerfile Examples

#### Linux (Dockerfile.linux)

```dockerfile
FROM python:3.11-slim as builder

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3-dev \
    python3-pip \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-ttf-dev \
    libsdl2-mixer-dev \
    && rm -rf /var/lib/apt/lists/*

# Create virtual environment
RUN python -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

# Install PySDL2 and dependencies
RUN pip install --no-cache-dir PySDL2 PySDL2-dll

# Copy source and build
WORKDIR /src
COPY . .
RUN pip install pyinstaller

# Build with PyInstaller
RUN pyinstaller --onefile --windowed --name BobMan-linux main.py

# Final stage
FROM ubuntu:22.04
COPY --from=builder /src/dist/BobMan-linux /app/BobMan
ENTRYPOINT ["/app/BobMan"]
```

#### macOS (Dockerfile.macos)

```dockerfile
# Use osxcross for cross-compilation
FROM ghcr.io/cimg/python:3.11 as builder

# Install osxcross and macOS SDL2 dependencies
# (This requires pre-built osxcross environment with macOS target)

RUN pip install PySDL2 pyinstaller

WORKDIR /src
COPY . .

# Build with PyInstaller for macOS
RUN pyinstaller --onefile --windowed --name BobMan-mac --target-arch x86_64 main.py

FROM alpine:latest
COPY --from=builder /src/dist/BobMan-mac /app/BobMan
ENTRYPOINT ["/app/BobMan"]
```

#### Windows (Dockerfile.windows)

```dockerfile
# Use mingw-w64 for Windows cross-compilation
FROM ubuntu:22.04 as builder

# Install mingw-w64 and SDL2 for Windows
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 \
    python3-pip \
    python3-venv \
    mingw-w64 \
    && rm -rf /var/lib/apt/lists/*

# Install PySDL2 with Windows support
RUN pip install PySDL2 pyinstaller

WORKDIR /src
COPY . .

# Build with PyInstaller for Windows
RUN pyinstaller --onefile --windowed --name BobMan-windows.exe main.py

FROM alpine:latest
COPY --from=builder /src/dist/BobMan-windows.exe /app/BobMan.exe
ENTRYPOINT ["/app/BobMan.exe"]
```

### Multi-Platform Build Script (build-all.sh)

```bash
#!/bin/bash
set -e

# Build for Linux
docker build -f docker/Dockerfile.linux -t bobman-linux .
docker create --name bobman-linux-temp bobman-linux
mkdir -p build/linux
docker cp bobman-linux-temp:/app/BobMan build/linux/BobMan
docker rm bobman-linux-temp

# Build for macOS (requires macOS cross-compile setup)
# docker build -f docker/Dockerfile.macos -t bobman-macos .
# docker create --name bobman-macos-temp bobman-macos
# mkdir -p build/macos
# docker cp bobman-macos-temp:/app/BobMan build/macos/BobMan
# docker rm bobman-macos-temp

# Build for Windows
docker build -f docker/Dockerfile.windows -t bobman-windows .
docker create --name bobman-windows-temp bobman-windows
mkdir -p build/windows
docker cp bobman-windows-temp:/app/BobMan.exe build/windows/BobMan.exe
docker rm bobman-windows-temp

echo "Builds complete in build/ directory"
```

### Buildx Multi-Platform Build

For true multi-platform Docker builds:

```bash
# Set up Buildx
docker buildx create --use --name multiarch-builder

# Build and export for all platforms
docker buildx build \
  --platform linux/amd64,linux/arm64,darwin/amd64,darwin/arm64,windows/amd64 \
  -t bobman:latest \
  --output "type=local,dest=build" \
  .
```

Note: Windows builds via Docker on non-Windows hosts require Wine or cross-compilation toolchain.

### GitHub Actions CI/CD

Create `.github/workflows/build.yml` for automated cross-platform builds:

```yaml
name: Build

on:
  push:
    branches: [ main, python ]
  pull_request:
    branches: [ main, python ]

jobs:
  build-linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.11'
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev
          pip install PySDL2 pyinstaller
      - name: Build
        run: |
          pyinstaller --onefile --windowed --name BobMan-linux main.py
          mkdir -p build/linux
          cp dist/BobMan-linux build/linux/
      - name: Upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: BobMan-linux
          path: build/linux/

  build-macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.11'
      - name: Install dependencies
        run: |
          brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer
          pip install PySDL2 pyinstaller
      - name: Build
        run: |
          pyinstaller --onefile --windowed --name BobMan-mac main.py
          mkdir -p build/macos
          cp dist/BobMan-mac build/macos/
      - name: Upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: BobMan-mac
          path: build/macos/

  build-windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Set up Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.11'
      - name: Install dependencies
        run: |
          pip install PySDL2 pyinstaller
      - name: Build
        run: |
          pyinstaller --onefile --windowed --name BobMan-windows.exe main.py
          mkdir -p build/windows
          cp dist/BobMan-windows.exe build/windows/
      - name: Upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: BobMan-windows
          path: build/windows/
```

This provides automated builds for all three platforms on every push/PR.

### Platform-Specific Considerations

| Platform | Build Method | Notes |
|----------|--------------|-------|
| **Linux** | Native Docker build | Works directly, produce static or dynamic binaries |
| **macOS** | Cross-compile with osxcross | Requires osxcross setup, or build natively on macOS |
| **Windows** | Cross-compile with mingw-w64 | Produces .exe files that run on Windows |

### Native Builds (Without Docker)

#### Linux
```bash
# Install dependencies
sudo apt install python3-dev python3-venv libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev

# Build
python -m venv venv
source venv/bin/activate
pip install PySDL2 pyinstaller
pyinstaller --onefile --windowed main.py
```

#### macOS
```bash
# Install dependencies (using Homebrew)
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer

# Build
python -m venv venv
source venv/bin/activate
pip install PySDL2 pyinstaller
pyinstaller --onefile --windowed --icon=data/icon.icns main.py
```

#### Windows
```bash
# Install dependencies (using Chocolatey or vcpkg)
choco install python sdl2

# Build
python -m venv venv
venv\Scripts\activate
pip install PySDL2 pyinstaller
pyinstaller --onefile --windowed --icon=data/icon.ico main.py
```

---

## Packaging

For distribution:

```bash
# Using PyInstaller for standalone executable
pip install pyinstaller
pyinstaller --onefile --windowed main.py

# Or with UPX compression
pyinstaller --onefile --windowed --upx-dir=/path/to/upx main.py
```

---

## Timeline Estimate

| Phase | Duration | Priority |
|-------|----------|----------|
| 1. Skeleton | 1 day | High |
| 2. Resources | 1-2 days | High |
| 3. Game Loop | 2-3 days | High |
| 4. Entities | 2 days | High |
| 5. Audio | 1 day | Medium |
| 6. Features | 2-3 days | Medium |
| 7. Menu | 2 days | Medium |
| 8. Editor | 3-4 days | Low |
| 9. Docker Build System | 2 days | High |
| **Total** | **16-21 days** | |

---

## Getting Started

1. Clone the repository
2. Create and activate virtual environment (see Installation above)
3. Run `python main.py` to test basic setup
4. Begin porting module by module

---

## Resources

- [PySDL2 Documentation](https://pysdl2.readthedocs.io/)
- [SDL2 Documentation](https://wiki.libsdl.org/) (same API as PySDL2)
- [PySDL2 Tutorial for Pygame Users](https://pysdl2.readthedocs.io/en/latest/tutorial/pygamers.html)
