# Keegan Product Improvement Spec

To move Keegan from "Tech Demo" to "Polished Product", we will engage in three tracks of work: **Experience**, **Engineering**, and **Content**.

## 1. Experience & Onboarding

### A. "Host a Station" Onboarding
Currently, Keegan is a "personal" ghost. We want to enable users to use it as a broadcast source or a shared vibe.

- **Web UI Wizard**:
  - Add a `/host` route in the React app.
  - **Step 1: Identity**: Let users name their station (e.g., "Neon Cafe", "Sleeper Ship").
  - **Step 2: Scheduling**: Simple day/night cycle configurator (e.g., "Switch to Arcade Mode at 8 PM").
  - **Step 3: Broadcast**: Present the "Ingest" options (see below).

### B. Mobile "Remote Control" & Listening
Allow users to control the vibe or listen in from their phone while the PC runs the core engine.

- **"Copy All Ingest URLs"**:
  - Add a prominent "Share" button in the Web Header.
  - **Action**: Generates a clipboard payload with:
    - **Local Control URL**: `http://<LAN_IP>:3000` (for remote controlling the mood).
    - **Audio Stream URL**: (Future) If we implement an HTTP audio stream endpoint in C++.
  - **QR Code**: Render a QR code for the LAN Control URL so a user can instantly adjust the mood from their phone without installing an app.

### C. "App Frequency" + Shared Tone Rooms
When a user opens a specific app (or activity), the engine auto-tunes a frequency that places them into a shared “tone room” with others on the same app. This creates spontaneous co-presence: “everyone in VS Code right now is drifting the same Focus tone.”

#### Concept
- **App → Tone → Frequency** mapping.
- The EXE detects the active app, selects a tone profile, and tunes to a shared “room frequency.”
- Other listeners on the same app/tone hear a coherent soundscape, even if they aren’t speaking (ambient “radio room”).

#### Goals
- **Instant belonging**: Open an app, drop into a matching room.
- **Subtle social**: You can feel others are “there” without heavy chat.
- **Low friction**: No manual station setup required; the app auto-assigns a room.

#### Room model
- **Room ID** = `region + app_key + tone_id + day_seed`
- **Frequency** = deterministic hash of the room ID (so users converge on the same frequency).
- **Day seed** rotates daily so rooms feel alive (e.g., new micro-variations each day).
- Optional “micro‑rooms” for smaller subsets (timezone or cohort).

#### App detection
- Windows active app process name + window title (existing heuristics).
- Normalize to `app_key` (e.g., `code`, `chrome`, `discord`, `blender`).
- Map `app_key` to `tone_id` via config table:
  - `code` → `focus_room`
  - `photoshop` → `arcade_glow`
  - `discord` → `arcade_social`
  - `spotify` → `rain_cave`

#### Tone profile
- A **tone** is a named config bundle:
  - base stem recipe
  - energy curve
  - reverb/color palette
  - micro-variations seed
- Users can override via mod packs (local customization).

#### Frequency assignment
- Use a stable hash of room ID → float within FM band (87.0–108.0).
- Round to 0.1 MHz increments.
- Avoid collisions by stepping if occupied.
- Display the frequency + room label in UI (“Focus Room · 98.7 MHz”).

#### Shared social layer (v1)
- Passive: registry counts listeners per room.
- Ambient: show “X listeners in this room” in UI.
- Optional: “signal pulse” visualization that subtly intensifies with listener count.

#### Optional active broadcast (v2)
- If a user opts in, they can transmit a mic/voice layer into the room.
- Most users remain “listener only,” preserving ambient vibe.

#### Privacy
- No raw app title in registry, only normalized `app_key`.
- Hash room identifiers; don’t log personal app titles.
- Local opt-out or “private mode” (disables app-based tuning).

#### Data model (registry)
```
roomId: "us-midwest|code|focus_room|2026-02-01"
roomFrequency: 98.7
roomListeners: 42
toneId: "focus_room"
appKey: "code"
```

#### Registry schema additions
- Store room presence separate from stations to avoid leaking personal metadata.
- New collection: `rooms` keyed by `roomId`.
```
{
  "roomId": "us-midwest|code|focus_room|2026-02-01",
  "region": "us-midwest",
  "appKey": "code",
  "toneId": "focus_room",
  "frequency": 98.7,
  "listenerCount": 42,
  "lastSeen": 1738420000000
}
```

#### API endpoints (registry)
```
GET  /api/rooms?region=us-midwest&appKey=code
POST /api/rooms/<roomId>/presence
GET  /api/rooms/<roomId>
```

**POST /api/rooms/<roomId>/presence** body:
```
{
  "listenerId": "listener_123",
  "action": "join" | "heartbeat" | "leave",
  "region": "us-midwest",
  "appKey": "code",
  "toneId": "focus_room",
  "frequency": 98.7
}
```
Response:
```
{
  "roomId": "us-midwest|code|focus_room|2026-02-01",
  "listenerCount": 42,
  "lastSeen": 1738420000000
}
```

#### Client behavior (EXE)
- On active app change:
  - Calculate `roomId` + `frequency`.
  - Tune locally to `frequency`.
  - POST `presence` with `action=join`.
- Every 15s:
  - POST `presence` with `action=heartbeat`.
- On app change or shutdown:
  - POST `presence` with `action=leave` for previous room.

#### UI behavior (Web)
- Add a “Rooms” tab in the Regional Directory.
- Allow filtering by app key and tone.
- Clicking a room sets the tuner to that frequency and reveals “who’s here” count.

#### Failure modes
- If registry is offline: keep local tuning and show “room unavailable.”
- If room collisions occur: apply deterministic offset (e.g., +0.1 MHz steps) until free.

#### Security + abuse controls
- Rate limit `presence` per IP / listenerId.
- Expire listeners after 30s of no heartbeat.
- Optional: require `KEEGAN_REGISTRY_KEY` for presence writes in hosted deployments.

#### UI additions
- **Room badge**: “FOCUS ROOM · code”
- **Join button**: quick tune to the room frequency.
- **Room status**: listeners + pulse indicator.
- **App lock**: toggle to force the current app’s room even if app focus changes.

#### Engine flow (pseudocode)
```
onActiveAppChange(appKey):
  tone = toneMap[appKey] || defaultTone
  roomId = hash(region, appKey, tone, date)
  frequency = frequencyFor(roomId)
  tune(frequency)
  setTone(tone)
  registry.postRoomPresence(roomId, frequency, tone, appKey)
```

## 2. Engineering & Architecture

### A. Fix the Voice Pipeline (Critical)
The current `StoryGenerator` gets text from the LLM but fakes the audio.

- **Problem**: `story_generator.cpp` receives JSON but lacks a Base64 decoder or a binary download mechanism for the generated TTS.
- **Solution**:
  1.  **Update `llm_router`**: Ensure it returns a URL to a generated WAV/MP3 (e.g., serving static files from a cache) OR returns raw binary.
  2.  **C++ Fetch**: Use `httplib` to download the audio file to `cache/stories/<id>.wav`.
  3.  **Playback**: Point the `miniaudio` decoder to this new file instead of the placeholder assets.

### B. "Ingest" API & Audio Streaming
To truly "Host a Station", other devices need to hear it.

- **Icecast/Shoutcast Client**: Implement a `StreamOutput` class in C++ that encodes the final mix (MP3/OGG) and pushes it to an Icecast server.
- **Local HTTP Stream**: (Simpler) creating a `/stream.mp3` endpoint in `web_server.cpp` that streams the ring buffer. This allows mobile phones to "tune in" via browser.

### C. Persistent "Station State"
- Save user preferences (Station Name, Schedule) to a `config/user.json` so personality persists across restarts.

## 3. Content & Quality

### A. "The Writer" (LLM Prompts)
We need better system prompts for the `llm_router` to generate distinct voices per mood.
- **Focus**: Concise, observational, reassuring.
- **Arcade**: Cyberpunk, glitchy, slightly paranoid.
- **Sleep**: Abstract, dream-logic, slow cadence.

### B. DSP Polish
- **Master Limiter**: Ensure the summation of 5 layers never clips, even during transitions.
- **Drift**: Implement the "Parameter Drift" logic where parameters like "Reverb Wetness" slowly wander over 10-minute windows to prevent fatigue.

---

## Recommended Immediate Next Steps

1.  **Web**: Implement the **QR Code / Share** modal. It's high impact, low effort.
2.  **C++**: Implement **Binary Audio Download** in `StoryGenerator` to make the AI voice real.
3.  **Content**: Author 3 distinct system prompts for the `llm_router` config.
