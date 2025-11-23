# Keegan: Tiny Local AI Radio
**Concept, Design System, and Production Plan**

## Logo & Identity
*   Glyph: small square/circle with two thin inward-curved pillars, a centered floating dot, short horizontal strokes above/below - reads like an open book fused with a brain node.
*   Style: black or charcoal on off-white, minimal, no gradients, no round/cute edges.
*   Usage: tray icon, docs, and splash assets use `assets/logo.png` (monochrome); pulse animation on the tray reflects current energy.

## 1. Concept: "Keegan"
Keegan is a digital spirit that lives in your system tray. It is not a tool, but a companion that "haunts" your audio output with reactive atmospheres. It doesn't play songs; it weaves moods. It observes your digital presence and adjusts the sonic environment to match or enhance your state.

## Quality Guardrails (keep it simple)
*   No chatty AI. Only rare one-line micro stories.
*   Audio thread never waits on AI or disk; optional workers are disposable.
*   Tray-only UI. No popups, no feeds, no timelines.
*   Gentle dynamics: equal-power crossfades, no harsh surprises, no clipping.
*   Fail soft: missing stems mute that role and log once.

### Core Philosophy
*   **Alive, Not Static**: The audio never loops identically. It evolves.
*   **Mood, Not Genre**: Users select "Focus" or "Sleep", not "Lo-Fi" or "Ambient".
*   **Companion, Not App**: Minimal UI. Interaction is like nudging a sleeping pet.

## 2. Experience Design

### User Interface (The "Creature")
*   **Tray Icon**: The primary interface. A small, pulsing glyph.
    *   *Pulse Rate*: Indicates energy level.
    *   *Color*: Indicates current mood (Amber=Focus, Blue=Rain, Neon=Arcade, Indigo=Sleep Ship).
*   **Interaction**:
    *   *Left Click*: Toggles Mute/Unmute (wakes up/sleeps).
    *   *Right Click*: Opens minimal context menu for Mood Selection and "Nudge" (force transition).
    *   *Hover*: Shows a tooltip with a cryptic, poetic status (e.g., "Dreaming of electric rain...").

### The Moods (MVP)
1.  **Focus Room**:
    *   *Vibe*: Warm library, ticking clocks, distant writing, soft cello.
    *   *Color*: Amber/Sepia.
    *   *Function*: Low distraction, steady rhythm.
    *   *Audio feel*: gentle tick/wood/paper textures; low shimmer reverb.
2.  **Rain Cave**:
    *   *Vibe*: Subterranean water, echoing drips, deep drone, wind chimes.
    *   *Color*: Deep Blue/Teal.
    *   *Function*: Relaxation, isolation.
    *   *Audio feel*: long decays with low-cut verb; sparse drips, air hiss.
3.  **Arcade Night**:
    *   *Vibe*: Neon hum, distant coin drops, muffled synthwave bass, street rain.
    *   *Color*: Hot Pink/Purple.
    *   *Function*: Energy, creative flow.
    *   *Audio feel*: muted bass pulse, short gated verb, occasional chimes/bleeps.
4.  **Sleep Ship**:
    *   *Vibe*: Quiet starship cabin, slow engine thrum, soft ventilation hiss, distant hull creaks.
    *   *Color*: Slate/Deep Indigo.
    *   *Function*: Calm descent, minimal motion, gentle warmth.
    *   *Audio feel*: indigo/charcoal bed, filtered hiss, rare creaks, very low density.

### Micro-Stories
*   Rarely (every 15-45 mins), Keegan whispers a single sentence.
*   *Content*: Abstract, evocative, non-sequitur. "The data streams are cold tonight."
*   *Delivery*: Heavily processed TTS (reverb, delay, pitch-shift) to sound like a transmission; mixed quietly and ducked 6-9 dB.

## 3. Behavior Brain (The State Machine)

The Brain determines the `TargetState` based on inputs.

### Inputs
*   **Time of Day**: Morning (High Energy), Night (Low Energy).
*   **System Activity**: Keyboard/Mouse events (High activity = higher density/tempo).
*   **Active App Heuristics**: Foreground process class boosts/suppresses mood weights (e.g., IDE boosts Focus, game boosts Arcade).
*   **User Sliders**: Intensity (0.0 - 1.0).

### Internal Variables
*   `Energy` (0.0 - 1.0): Controls tempo and rhythm volume.
*   `Density` (0.0 - 1.0): Controls number of active layers.
*   `Tension` (0.0 - 1.0): Controls dissonance and FX wetness.

### Logic
*   *Transition Engine*: Never snaps. Interpolates `CurrentState` to `TargetState` over 5-30 seconds.
*   *Pattern Evolution*: Tracks "Time in Mood". After 1 hour in Focus, might drift slightly darker or lighter automatically.
*   *App Heuristic Weights*: Lightweight mapping table `{process_regex -> mood_bias, energy_bias}` evaluated every ~5 seconds, low-pass filtered so games/calls can gently push energy up while docs/IDEs nudge focus.
*   *Personality Drift*: Small per-day offsets to energy/warmth/tension stored in config, clamped to avoid runaway.

## 4. Audio Engine Architecture

A multilayer generative mixer.

### Layers
1.  **Bed (Drone)**: Continuous loop, pitch-shifted slowly.
2.  **Environment**: Stochastic one-shots (rain, clicks, wind).
3.  **Rhythm**: Soft, repetitive loops (filtered noise, pulses).
4.  **Melodic**: Procedural sequences or long stem loops.
5.  **Voice**: The TTS interrupt layer.

### Audio Loop (hard requirements)
*   Threading: audio thread renders from prebuilt schedules; no allocations, no file I/O, no locks in callback.
*   Scheduler: 30-60ms lookahead, tempo-aware; emits events for stems/synth/TTS respecting density curves.
*   Crossfades: equal-power for layer swaps and mood transitions; reverb tail preserved during switches.
*   Ducking: sidechain on voice bus with RMS detector, soft knee, attack 10-20ms, release 300-500ms; avoid pumping.
*   Reverb: light plate with predelay and damping; input low-cut; per-mood wet presets.
*   Safety: soft limiter at -1 dBFS; per-bus gain staging; denormal guard on filters.

### Density Curves (Refined)
*   **Focus Room**: Slow S-curve (start at 0.35, settle 0.55), tiny oscillation +/-0.05 every 90s.
*   **Rain Cave**: Low-flat curve (0.25-0.4) with gentle random dips to 0.15 to create space.
*   **Arcade Night**: Rising ramp (0.4 -> 0.75 over 4-6 min) then micro-drops every 30-45s to let surprises breathe.
*   **Sleep Ship**: Very low plateau (0.15-0.25) with rare swells to 0.35 tied to distant creaks; long release to avoid abruptness.

### The Recipe Format (JSON)
```json
{
  "mood": "rain_cave",
  "bpm": 60,
  "layers": [
    { "type": "drone", "file": "deep_water.wav", "vol": 0.8 },
    { "type": "scatter", "file": "drip_*.wav", "density": 0.4, "pan_spread": 1.0 }
  ],
  "transitions": ["focus_room"]
}
```

## 5. C++ System Architecture

### Tech Stack
*   **Language**: C++20
*   **Audio**: `miniaudio` (single header, low latency, cross-platform).
*   **Config**: `vjson` (reused from previous project).
*   **UI**: Win32 API (Shell_NotifyIcon) for tray, or a lightweight wrapper like `tray.hpp`.

### Modules
1.  **`Main`**: Entry point, tray loop, system hooks.
2.  **`Brain`**: State machine, processes inputs, updates `EngineParams`.
3.  **`AudioEngine`**: 
    *   Owns the `miniaudio` context.
    *   `Mixer`: Sums layers.
    *   `VoiceManager`: Handles TTS generation/playback.
4.  **`ContentManager`**: Loads JSON recipes and audio assets into memory.

### DSP Polish Targets
*   **Reverb**: Lightweight plate/FDN hybrid (pre-delay 20-60ms, damped high shelf, modulated decay). Per-mood wet levels map to `warmth`/`tension`.
*   **Ducking**: Sidechain compressor keyed by TTS/voice bus; attack 10-20ms, release 300-500ms, ratio ~2.5:1, makeup off. Optional gentle duck on narrative one-shots.
*   **Crossfades**: Equal-power crossfades for layer swaps; tempo-aware loop points; fade tails keep reverb continuity during transitions.
*   **Filters**: Per-layer biquads (LP/HP) with smooth parameter ramps to avoid zippering when density/energy change.
*   **Limiter**: Soft ceiling at -1 dBFS on master to prevent clips.

### Testing & Verification (must-have)
*   Config validation: schema check with clear errors; missing/malformed fields default safely.
*   Render sanity: offline 10s renders per mood; assert no clipping, RMS within expected range.
*   Timing: jitter histogram on scheduler and audio thread budget; must stay under block deadline at target buffer size.
*   Transition tests: state machine respects allowed transitions and fades complete within configured duration.

## 6. Production Plan (MVP)

### Phase 1: The Heartbeat (Audio Core)
*   Set up CMake project.
*   Integrate `miniaudio`.
*   Implement basic multi-track playback (Drone + Random One-shots).
*   **Goal**: Run app, hear a drone and random rain sounds.

### Phase 2: The Mind (Configuration & Brain)
*   Integrate `vjson`.
*   Define `Mood` structs.
*   Implement `Brain` class to interpolate volume/density.
*   **Goal**: App reads JSON, crossfades between two hardcoded moods.

### Phase 3: The Body (Tray UI)
*   Implement Windows Tray Icon.
*   Add context menu (Mood Select).
*   **Goal**: Control the app from the tray.

### Phase 4: The Soul (Content & Polish)
*   Source/Generate audio stems for the 3 MVP moods.
*   Implement the "Micro-story" text generator (array of strings for MVP).
*   Add simple DSP (Low pass filter based on 'muffle' state).

## 7. Future Expansion (Post-MVP)
*   **AI Worker**: Local LLM (Gemma/Phi) generating stories on the fly.
*   **Plugin Host**: Lua scripting for custom procedural audio.
*   **Visuals**: Desktop overlay particles.
