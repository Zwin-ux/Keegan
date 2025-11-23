# Keegan (AI Radio)

![Keegan glyph](assets/logo.png)

Tray-first local AI radio that weaves moods instead of playlists. Lives in Windows system tray, runs offline, and drifts through recipes of stems, procedural synth, and tiny whispered stories.

## Aesthetic (Alpha)
- Tray icon uses the glyph; pulse rate = energy; mood colors: Focus amber, Rain blue, Arcade neon magenta, Sleep indigo.
- Audio feel: smooth equal-power fades, quiet micro-stories with ducking, light plate reverb (low-cut), no clipping (soft limiter).
- Mood textures: Focus (ticks/wood/paper), Rain (water/air/stone), Arcade (muted bass + bleeps), Sleep (engine/hiss/creaks).

## What it does
- Tray controls: play/pause, intensity, mood select (Focus Room, Rain Cave, Arcade Night, Sleep Ship).
- Mood brain: state machine with smooth transitions, personality drift over days, and active-app weighting (IDE biases focus, games bias arcade, media/idle bias sleep/rain).
- Audio engine: miniaudio-based mixer with layer scheduler, gentle crossfades, per-layer filters, ducked TTS/voice bus, and lightweight plate-style reverb.
- Content: bundled stems plus procedural synth; micro-stories come from a prewritten list for MVP.

## Density curves (per mood)
- Focus Room: slow S-curve (0.35 -> 0.55) with tiny +/-0.05 wobble every ~90s.
- Rain Cave: low-flat (0.25-0.4) with occasional dips to 0.15 for space.
- Arcade Night: rising ramp (0.4 -> 0.75 over 4-6 min) then micro-drops every 30-45s.
- Sleep Ship: low plateau (0.15-0.25) with rare swells to 0.35 and long releases.

## DSP polish targets
- Reverb: light plate/FDN hybrid, pre-delay 20-60ms, damped high shelf, modulated decay; wet mapped to warmth/tension per mood.
- Ducking: sidechain compressor keyed by voice/TTS bus; attack 10-20ms, release 300-500ms, gentle ratio ~2.5:1.
- Crossfades: equal-power fades for swaps and tempo-aware loop points; keep tails to avoid holes during transitions.
- Filters: per-layer LP/HP biquads with smoothed parameter ramps to avoid zipper noise.

## Inputs the brain watches
- Time of day, keyboard/mouse intensity, user intensity slider.
- Foreground app heuristics: regex table -> mood/energy biases, evaluated every few seconds with smoothing.
- Random seeds to keep recipes alive and allow rare surprises.

## Repo layout
- `assets/` - logo and bundled stems/tones (includes Sleep Ship placeholders and synth preset).
- `assets/tray_colors.json` - tray color/pulse config (Amber/Blue/Neon Magenta/Indigo).
- `src/` - Keegan C++ sources (brain, DSP, state machine).
- `vendor/` - `miniaudio.h`, `vjson/` for config loading.
- `config/` - mood pack JSON including Sleep Ship.
- `llm_router/` - co-located LLM router project (from sibling repo) for optional AI worker.
- `DESIGN.md` - concept and system design.
- `implementation_plan.md` - phased build checklist.

## Build (desktop)
```bash
cd ai_radio
cmake -S . -B build
cmake --build build --config Release
```
Run the demo: `./build/keegan` (prints simulated RMS and heuristic-driven mood changes). Replace stems/presets under `assets/` with real audio for production.

## Notes on LLM router
`llm_router/` is copied in for a future offline worker (e.g., micro-story generation). It builds as its own target via the nested `CMakeLists.txt`.
