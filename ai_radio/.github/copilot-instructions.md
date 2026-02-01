# Copilot Instructions for Keegan (AI Radio)

## Project Overview
Keegan is a local, tray-based AI radio application that generates reactive audio soundscapes ("moods") instead of playing songs.
- **Core (C++)**: Audio engine, state machine, and system integrators in `src/`.
- **Frontend (Web)**: React/Vite/Tailwind UI in `web/`, served by the embedded C++ server.
- **AI/LLM**: `llm_router/` submodule handles external LLM interactions; `api/` & scripts manage assets.

## Architecture & Code organization
- **Audio Engine (`src/audio/`)**: 
  - **Thread Safety**: The audio callback runs on a real-time thread. **Never** allocate memory, lock mutexes, or perform I/O in `audio::Engine::render` or its dependencies.
  - **DSP**: Uses `miniaudio.h`. Custom mixing architecture with 5 fixed layers: Bed (drone), Environment, Rhythm, Melodic, Voice.
- **Brain (`src/brain/`)**: Controls the "mood" state machine. Updates occur in the main loop, pushing parameter changes to the engine via atomic/lock-free mechanisms.
- **Configuration (`config/`)**: JSON-based. `moods.json` defines audio recipes; loaded via `src/config/mood_loader.cpp` using `vendor/vjson`.
- **Web UI (`web/`)**: Communicates with C++ backend via HTTP/REST (served by `src/ui/web_server.cpp`) to control playback and settings.

## Build & Run Workflows
- **C++ Core**:
  ```bash
  cmake -S . -B build
  cmake --build build --config Release
  ./build/Release/keegan.exe
  ```
- **Web Frontend**:
  ```bash
  cd web
  npm install
  npm run dev      # Development
  npm run build    # Production assets for embedding
  ```

## Key Conventions
- **DSP/Audio**:
  - Prefer gentle transitions (equal-power crossfades) over abrupt switches.
  - Use `miniaudio` data structures.
  - All synth/audio objects must have a `render(float* out, size_t frames)` method.
- **Hardware Integration**:
  - `src/brain/app_heuristics.cpp`: Maps foreground window classes/titles to mood biases (e.g., IDE -> Focus).
  - Tray icon reflects "Energy" (pulse rate) and "Mood" (color).
- **Paths**: The binary expects to run from the project root or finding assets relative to the executable. `util::fixWorkingDirectory()` attempts to stabilize this.

## Integration Points
- **LLM Router**: The `llm_router/` directory is a self-contained CMake project. Referenced but built separately or as a sub-target.
- **Server**: C++ `uisrv::WebServer` binds to port 3000 (default) to serve the UI and handle API calls from the React frontend.

## Common Tasks
- **Adding a Mood**: Update `config/moods.json` with new density curves and `assets/` references.
- **New Audio Effect**: Implement in `src/audio/`, expose in `src/audio/engine.h`, bridge parameters in `src/brain/`.
- **Web Feature**: Add component in `web/src/`, add API endpoint in `src/ui/web_server.cpp`.
