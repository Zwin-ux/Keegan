# Keegan Radioverse Spec

## Purpose
Keegan is an open-source radio engine with a social web layer. The EXE is the audio brain. The web platform is discovery, chat, and stations. The product goal is to let anyone host a station, mod their frequency, and broadcast their local computer vibe.

## Current baseline (already in repo)
- C++20 audio engine with moods, stems, and heuristics (`src/audio`, `src/brain`).
- Mood pack JSON (`config/moods.json`) with stems under `assets/stems`.
- Local HTTP server for state and UI hosting (`src/ui/web_server.*`).
- Web tuner UI concept with static and ghost stations (`web/src`).
- Optional story generator stub (`llm_server.py`, `api/index.py`).

## Product architecture
### Engine (EXE)
- Generates audio from stems + procedural layers.
- Applies heuristics (active app, activity, time of day).
- Exposes a local HTTP API for state and control.
- Provides a privacy-safe "vibe vector" for broadcasting metadata.

### Local bridge
- A local API for UI and streaming control.
- Web UI can connect to the EXE on localhost.
- Websocket channel for live metadata updates.

### Web platform
- Station directory, search, and regional discovery.
- Station pages with chat, schedule, and show archives.
- Identity, follow graph, and social mechanics.

### Streaming
- Primary ingest: WebRTC for low latency.
- Fallback: LL-HLS for scale and mobile compatibility.
- Metadata via websocket to keep chat and visuals in sync.

### Mobile
- Mobile listeners use LL-HLS with near-real-time chat.
- Optional WebRTC for "call-in" sessions and live co-hosts.

## Regional frequencies
- Frequencies are unique per region (not global).
- Region is chosen at station creation and used for discovery.
- Example: 98.7 can exist in multiple regions.

## The "computer vibe" engine
- Local signals only: active process, input activity, time of day.
- These signals form a Vibe Vector with 6-8 values.
- Only the Vibe Vector is shared with the network.
- No raw app list or keystroke data leaves the device.

## Open-source modding model
### Frequency packs
A frequency pack is a mod that defines a station identity and mood data.
- Pack contains `manifest.json`, `moods.json`, and assets.
- Pack can remix stems, density curves, and narrative frequency.
- Pack can declare its frequency seed and regional mapping.

### Compatibility
- For now, packs should include the core mood IDs used by the UI
  (focus_room, rain_cave, arcade_night, sleep_ship).
- The UI will become data-driven after the mod loader lands.

## Social and gamification
- Listener XP for time tuned, reactions, and boosts.
- Host XP for retention, schedule consistency, and ratings.
- Weekly regional charts and "radio raids".
- Co-listen rooms with shared chat and queue control.

## Licensing
- Default mode uses bundled stems or procedural audio.
- User-supplied music requires user licensing compliance.
- Optional talk-only mode for zero licensing risk.

## Security and privacy
- Local bridge is localhost-only by default.
- Tokens for broadcast are short-lived.
- Vibe Vector is the only auto-shared metadata.
- Telemetry is opt-in and summarized.

## Milestones
### M0 (done)
- Local mood engine, stems, basic heuristics.
- Local web server with /api/state.

### M1 (near-term)
- Mod pack format and loader.
- Local bridge API extended.
- Regional station registry and directory.

### M2
- WebRTC ingest and LL-HLS playback.
- Station pages, chat, and schedules.
- Mobile listener MVP (PWA or native shell).

### M3
- Social features, badges, regional charts.
- Creator analytics and monetization options.

