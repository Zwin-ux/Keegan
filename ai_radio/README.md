# Frequency (Project: Keenga)

![Keegan glyph](assets/logo.png)

Frequency is the listener-facing name for Keegan — a tray-first local radio engine that weaves moods instead of playlists. It lives in the Windows system tray, runs offline, and drifts through recipes of stems, procedural synth, and tiny whispered stories.

Website (local registry UI): http://localhost:8090/
Website (production): TBD

## Aesthetic (Alpha)
- Tray icon uses the glyph; pulse rate = energy; mood colors: Focus amber, Rain blue, Arcade neon magenta, Sleep indigo.
- Audio feel: smooth equal-power fades, quiet micro-stories with ducking, light plate reverb (low-cut), no clipping (soft limiter).
- Mood textures: Focus (ticks/wood/paper), Rain (water/air/stone), Arcade (muted bass + bleeps), Sleep (engine/hiss/creaks).

## What it does
- Tray controls: play/pause, intensity, mood select (Focus Room, Rain Cave, Arcade Night, Sleep Ship).
- Mood brain: state machine with smooth transitions, personality drift over days, and active-app weighting (IDE biases focus, games bias arcade, media/idle bias sleep/rain).
- Audio engine: miniaudio-based mixer with layer scheduler, gentle crossfades, per-layer filters, ducked TTS/voice bus, and lightweight plate-style reverb.
- Content: bundled stems plus procedural synth; micro-stories come from a prewritten list for MVP.

## Startup guide (the “get signal fast” version)
This is the path I use when I want the room to go live in under 10 minutes.

1) **Start the registry (the directory).**
```bash
cd ai_radio/server
python registry_server.py
```
Open the directory UI: http://localhost:8090/

2) **Start the web console (the listener view).**
```bash
cd ai_radio/web
npm install
npm run dev
```
Open: http://localhost:5173/  
You should see the “Regional Directory” panel populate once the registry is online.

3) **Build + run the engine.**
```bash
cd ai_radio
cmake -S . -B build
cmake --build build --config Release
```
Run the EXE from the repo root so it can serve `web/dist`:
```
ai_radio/build/Release/keegan_patched.exe
```

4) **If the directory doesn’t load:**
- Check that http://localhost:8090/health returns `{ ok: true, ... }`.
- Make sure the registry allows your UI origin (see `ALLOWED_ORIGINS` in `server/README.md`).
- The UI now shows the exact failure reason next to the REGISTRY label.

## Open source and modding
- Keegan is designed as a moddable radio engine. Frequency packs can replace moods, stems, and tuning curves.
- See `docs/MODDING_GUIDE.md` and `mods/example_neon` for the pack format.

## Security
- Set `KEEGAN_BRIDGE_KEY` to protect local POST endpoints.
- Set `KEEGAN_BROADCAST_SECRET` to sign per-station ingest tokens (defaults to bridge key).
- Set `KEEGAN_REGISTRY_KEY` to protect registry POST endpoints.

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
- `config/station.json` - local station metadata and registry URL.
- `mods/` - frequency packs (modding).
- `docs/` - specs and platform docs.
- `server/` - station registry service + minimal directory UI.
- `llm_router/` - co-located LLM router project (from sibling repo) for optional AI worker.
- `DESIGN.md` - concept and system design.
- `implementation_plan.md` - phased build checklist.

## Product specs
- `docs/RADIOVERSE_SPEC.md` - full platform vision and architecture.
- `docs/LOCAL_BRIDGE_API.md` - local API between EXE and web.
- `docs/REGISTRY_API.md` - station registry API and directory.
- `docs/STATION_HOSTING_SPEC.md` - how to host a station and join the directory.
- `docs/ROADMAP.md` - milestones for shipping the platform.

## Build (desktop)
```bash
cd ai_radio
cmake -S . -B build
cmake --build build --config Release
```
Run the demo: `./build/keegan` (prints simulated RMS and heuristic-driven mood changes). Replace stems/presets under `assets/` with real audio for production.

## Web UI
```bash
cd ai_radio/web
npm install
npm run dev
```
Default dev URL: http://localhost:5173/
Env overrides:
- `VITE_REGISTRY_URL` (registry base URL)
- `VITE_REGISTRY_KEY` (optional, if registry requires auth)
- `VITE_BRIDGE_URL` (local bridge base URL, default `http://localhost:3000`)
- `VITE_BRIDGE_KEY` (optional, if bridge requires WS/auth)
- `REGISTRY_URL` (used by `npm run check:registry`, defaults to `http://localhost:8090`)

## Ingest (local dev)
See `ai_radio/server/ingest/README.md` for MediaMTX setup.

## Notes on LLM router
`llm_router/` is copied in for a future offline worker (e.g., micro-story generation). It builds as its own target via the nested `CMakeLists.txt`.
