# Implementation Plan: Keegan

## Phase 1: Project Setup & Audio Core
- [x] **Directory Structure**: Create `src`, `assets`, `vendor`, `config`.
- [x] **Dependencies**: 
    - Include `miniaudio.h` (vendor placeholder).
    - Copy `vjson` from `llm_router`.
- [x] **CMake**: Setup `CMakeLists.txt` for Windows target `keegan` (subdir `llm_router` included).
- [x] **Audio Engine v0.1**:
    - DSP utilities: equal-power crossfade, light plate reverb, sidechain ducking.
    - Engine scaffold that renders synthetic layers per mood for testing.

## Phase 2: Configuration & Data
- [x] **Data Models**: Define `Mood`, `LayerConfig` structs and state machine.
- [x] **Loader**: Implement JSON loader using `vjson` for mood pack (`config/moods.json`).
- [ ] **Assets**: Add real WAV stems/presets for Focus/Rain/Arcade/Sleep.

## Phase 3: The Brain (Logic)
- [x] **Brain Class**: 
    - `update(dt)` loop.
    - `setMood(string name)`.
    - Parameter interpolation (smooth volume transitions).
- [x] **App Heuristics**: Foreground process regex -> mood/energy bias.

## Phase 4: System Tray UI (Windows)
- [ ] **Tray Wrapper**: Simple class wrapping `Shell_NotifyIcon`.
- [ ] **Menu**: CreatePopupMenu for mood switching.
- [ ] **Main Loop**: Integrate Win32 message loop with Audio thread.

## Phase 5: Content & Polish
- [ ] **Sound Design**: Generate/Find simple samples for Rain, Focus, Arcade, Sleep Ship; replace placeholders in `assets/stems/*`.
- [ ] **Story System**: Simple timed event triggering a specific voice sample (mock TTS for now).
- [x] **Sleep Ship Mood**: Add recipe, stems, and curves for the "sleep_ship" state with very low density and slow transitions.
- [x] **DSP Polish**: Implement light plate reverb, sidechain ducking for TTS/voice bus, and equal-power crossfades for layer swaps.
- [x] **Active-App Heuristics**: Map foreground process classes to mood/energy biases (e.g., IDE -> Focus, games -> Arcade, media -> Sleep/Rain) with smoothing.
