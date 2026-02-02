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

---

# Data + Tests Focused Update Plan

Goal: Make the next update measurable and repeatable. We should only ship changes that clearly improve retention, listening time, or successful hosting — and prove it with tests + telemetry.

## 1) Define the single “north star” metric
Pick one for the next release so we don’t overfit:
- **30‑minute retention** (best for vibe engines)
- **avg listening session length**
- **successful host rate** (people who get a live stream working)

## 2) Instrument the product (minimal but meaningful)
Add telemetry events in three surfaces: EXE, registry, web.

### EXE (local)
- `engine_start`
- `mood_change`
- `app_focus_change` (app_key only)
- `room_join`
- `room_leave`
- `broadcast_start`
- `broadcast_stop`
- `ingest_error`

### Registry
- `station_update`
- `listener_join`
- `listener_leave`
- `room_presence`

### Web
- `registry_health_ok/failed`
- `station_selected`
- `room_selected`
- `playback_start/stop`

**Data shape** (all events):
```
event: "room_join"
ts: 1738420000000
region: "us-midwest"
sessionId: "sess_123"
roomId: "us-midwest|code|focus_room|2026-02-01"
appKey: "code"
toneId: "focus_room"
```

## 3) Store it somewhere simple (MVP)
- **Local JSONL** for EXE (append‑only in `cache/telemetry.jsonl`)
- **Registry memory + daily JSON dump** for server (`data/telemetry-YYYY-MM-DD.json`)
- Optionally forward to a hosted endpoint later.

## 4) Tests that protect the metrics
Only add tests that directly protect the “north star”.

### Unit tests
- **Room frequency hash** is stable (same input → same frequency).
- **Room presence TTL** (no heartbeat → listener expires).
- **Token validation** (station-bound tokens reject cross‑station reuse).

### Integration tests
- **Registry health** returns `{ ok: true }`.
- **Station list** returns non-empty after an EXE heartbeat.
- **Room list** returns after presence posts.

### UI tests (smoke)
- Web UI renders stations if registry is up.
- Error banner shows exact failure type if registry is down.

## 5) Data‑driven change proposal (first update)
Focus on one change likely to move the north star. Example:

### Change: “Auto‑Join Rooms by App”
- Hypothesis: auto‑join increases session length because users feel passive co‑presence.
- Metric: avg listening session length.
- Experiment: random 50% auto‑join, 50% manual.
- Success: +15% session length or +10% retention.

## 6) Report after the release
Write a 1‑page internal post:
- What was shipped
- Hypothesis
- Actual metric movement
- Rollback / keep decision

## 7) Concrete sprint plan (2 weeks)
**Week 1**
- Add telemetry schema + local JSONL writer in EXE.
- Add `/api/telemetry` in registry (write to JSON file).
- Add test harness for registry (health + rooms).

**Week 2**
- Add A/B flag for auto‑join rooms.
- Add UI indicator (“Auto‑joined to Focus Room”).
- Read telemetry and compute metric deltas.

## 8) Deliverables checklist
- [ ] Telemetry events logged locally and in registry
- [ ] Unit tests for hashing + TTL + token validation
- [ ] Integration tests for registry endpoints
- [ ] A/B flag implementation and simple report

---

# Public Website + LAN Mode Plan

Goal: Ship a real public website while keeping the EXE local-control experience smooth. Support both public and LAN discovery.

## Architecture split (required)
- **Public web app** (Vercel): directory + listening + docs.
- **Public registry API** (Render): stations + rooms + telemetry.
- **Local bridge (EXE)**: engine control + live state + ingest tokens.

Why: A public website cannot talk to a user's localhost. The local bridge stays local, the registry stays public.

## Registry modes
- **Public registry**: hosted on Render, open for reads, gated for writes.
- **Local registry**: optional Python process on LAN for private stations.
- **UI toggle**: switch between Public / Local / Custom registry URL.

## LAN mode (local/public mix)
- **Auto-detect LAN**: EXE exposes its LAN IP in the web console.
- **QR code** for LAN listeners to open the local console.
- **Optional LAN registry**: run a local registry on the host machine for private rooms.

## Public website plan
- Deploy Vercel site from `ai_radio/web`.
- Build: `npm run build`, output `dist`.
- Env: `VITE_REGISTRY_URL=https://<render-service>.onrender.com`.
- Add a landing page section: "Listen now" and "Host a station".

## Render registry plan
- Service: Python web service from `ai_radio/server`.
- Start: `python registry_server.py`
- Env: `ALLOWED_ORIGINS=https://<vercel-site>.vercel.app,http://localhost:5173`
- Optional: `KEEGAN_TELEMETRY=1`

## Spam protection policy (public)
- **Reads:** open (no auth).
- **Writes:** require `KEEGAN_REGISTRY_KEY` in production.
- **Rate limit**: per-IP for `/api/rooms/*/presence` and `/api/stations`.
- **Moderation**: soft delete flag for stations.

## Room seed + frequency authority (fixes spec risks)
- Registry serves a **global day seed** (UTC) to avoid timezone splits.
- Registry assigns the **room frequency** on first presence.
- Clients never compute final frequency alone.

## Orchestration (dev)
- Single script `start_dev.ps1` or `start_dev.bat` to launch:
  - registry (8090)
  - web UI (5173)
  - optional EXE if built
- Script checks and creates `server/data/` if missing.

## Deliverables (public + LAN)
- [ ] UI toggle for registry source (Public/Local/Custom)
- [ ] Registry global seed endpoint
- [ ] Registry room frequency assignment
- [ ] LAN QR in console
- [ ] One-command dev start

---

# Next Features Spec (Web-First Hosting + EXE as Power-Up)

Goal: let creators host a show from the web without installing the EXE. The EXE remains an optional "vibe engine" that can enrich broadcasts locally.

## 1) Web-Hosted Radio Shows (No EXE Required)

### Concept
Creators can start a station from the web UI using their mic, browser audio, or an RTMP encoder. This makes Frequency usable as a pure web platform.

### Core flows
1. **Create Station** (web): name, region, description, tags, cover image.
2. **Go Live (Web)**: browser-based broadcast using WebRTC ingest.
3. **Directory Listing**: station appears in the public registry with live status.
4. **Listeners**: HLS/LL-HLS playback in the web UI.

### Web ingest architecture
- **Browser -> WebRTC ingest** (preferred for low setup):
  - Web client gets a signed ingest token.
  - Connects to a WebRTC ingest endpoint (MediaMTX v1).
- **Encoder -> RTMP ingest** (fallback):
  - Use tokenized RTMP URL.
  - MediaMTX converts to HLS.

### Minimal API needs
```
POST /api/stations/web/begin
  -> { stationId, ingest: { webrtcUrl, rtmpUrl, hlsUrl }, sessionId, token, expiresAt }

POST /api/stations/web/stop
  -> { ok: true }

GET /api/stations/:id/status
  -> { broadcasting, uptimeMs, bitrateKbps, listeners }
```

### Permissions / Safety
- Public reads, gated writes (registry key or creator auth).
- Station-bound tokens to prevent cross-station reuse.
- Rate limit presence + ingest requests.

## 1.1) Anonymous Frequency (One Global Open Slot)

### Concept
There is a single anonymous station anyone can go live on without creating an account. It is the "open mic" of Frequency. It is not 24/7 and is intentionally temporary.

### Rules
- Only one anonymous station exists (global), listed as `anon`.
- One live host at a time (single active session).
- If the host disconnects, the station goes idle after 60 seconds.
- Max live session length: **4 minutes** (tight, spontaneous drops).
- Cooldown per IP/device (default 10 minutes) after a session ends.

### UX flow
1. Click "Go Live (Anonymous)" in the Host panel.
2. Receive a short-lived token (10 min).
3. Start broadcasting via WebRTC.
4. Station appears immediately in the directory as "Anonymous Frequency".

### Visuals
- Anonymous hosts can upload a temporary cover image.
- If none is provided, a fixed anonymous avatar is used.

### Tokening
- `stationId = anon`
- Token scope: `station:anon`, `sessionId`, `exp`.
- Token is invalid if a different session is active.

### Moderation (soft)
- Basic rate limits + session duration caps.
- Manual kill switch (admin key) to stop a session.

## 1.2) Creator Stations (24/7)

### Concept
Persistent stations require a creator profile. This enables 24/7 hosting, stable station IDs, and future monetization.

### Minimal creator profile (v1)
- Handle + email (or magic link).
- Station name, description, region, tags.
- Optional cover image.

### Rules
- Creator stations can run indefinitely.
- Multiple sessions per station are not allowed without explicit co-host roles.
- Creator stations can set default schedule or "always on".

### Monetization (later)
- Paid 24/7 slots or boosted directory placement.
- Tips/subscriptions for hosts.

## 2) EXE as "Vibe Engine" Add-On

### Concept
EXE becomes a local enhancer: audio textures, procedural ambience, and optional co-host voice layers. It can publish into a web-hosted show.

### EXE features
- **Vibe Layer Output**: EXE generates an audio layer and publishes as RTMP.
- **Mood Automation**: auto-mood shifts based on app focus.
- **Local DSP**: provides an "analog warmth" profile to the stream.

### Integration idea
- Web station can "attach" a vibe engine:
  - EXE authenticates and claims a station.
  - EXE can update station mood/energy in registry.

## 3) Web Host Studio (UI)

### Panel layout
- **Mic input + levels**
- **Live status + uptime**
- **Station metadata**
- **Audience metrics** (listeners, region map)
- **Co-host link** (invite link to join broadcast)

### MVP: Web Audio Mixer
- 1 mic input
- 1 music bed (local file upload or stream URL)
- Simple compressor + limiter in browser

## 4) Open Hosting Pipeline (Server)

### Minimal stack
- **Registry** (Render): stations/rooms/listeners
- **Ingest** (MediaMTX on Render/Fly): RTMP + WebRTC -> HLS
- **Web UI** (Vercel): host/monitor/listen

### Deployment targets (v1)
- MediaMTX on Render/Fly for ingest.
- HLS served from the ingest service.
- Vercel web app reads public registry + HLS URLs.

## 5) Next Engineering Milestones

### Phase A: Web Host MVP
- Web UI "Host a station" flow.
- WebRTC ingest token issuance.
- Registry station create + heartbeat.

### Phase B: Vibe Engine Link
- EXE station claim flow (token + stationId).
- EXE posts mood/energy/now-playing.

### Phase C: Social Broadcasts
- Co-host join links with roles (host/cohost/producer).
- Chat or "Signal pulse" reactions.

---

# Decisions (v1 defaults)

- Ingest: WebRTC-first, RTMP fallback.
- Ingest server: MediaMTX.
- Listing: instant for now.
- Anonymous: one global open station (single active host).
- Creator requirement: only for persistent 24/7 stations.
- Co-hosts: single-host only in v1 (roles added later).

## Scale targets (v1 demo)
- 20 concurrent stations (including anon)
- 150 concurrent listeners
- 5 active hosts at the same time

---

# Open Questions (Answer these to finalize the roadmap)

1) Anonymous frequency: should it be **global** or **one per region**?
2) Anonymous station time limit: keep 30 minutes or change it?
3) Should anonymous hosts be able to upload a cover image, or keep it fixed?
4) Do we allow co-hosts in v1, or keep it single-host only?

---

# System Relationship Spec (Fly + Render + Vercel + EXE)

Goal: define the lean, reliable relationship between the public web UI (Vercel), registry API (Render), ingest server (Fly), and the optional EXE add-on. This is the minimum architecture that keeps the data model consistent and avoids unnecessary coupling.

## 1) Roles and Sources of Truth

- **Vercel (Web UI)**: UI only. No state, no persistence. Talks to Render (registry) and plays HLS from Fly.
- **Render (Registry API)**: source of truth for stations, rooms, sessions, listener counts. Controls auth + token issuance.
- **Fly (Ingest / MediaMTX)**: transport only. No station metadata. Takes RTMP/WebRTC and exposes HLS.
- **EXE (Local Engine Add-on)**: optional producer of station metadata + audio layer. Uses Render to register and update a station.

## 2) Data Model (Lean)

**Station** (registry owns this)
```
id, name, description, region, frequency, streamUrl,
status, broadcasting, coverImage,
broadcastMode, broadcastSessionId, broadcastStartedAtMs,
updatedAt, lastSeen
```

**Session** (registry owns this; short-lived)
```
sessionId, stationId, mode (anon|creator|exe),
token, startedAtMs, endsAtMs, clientId
```

**Listener presence** (registry owns this; ephemeral)
```
stationId, listenerId, lastSeen
```

**Room presence** (registry owns this; ephemeral)
```
roomId, listenerId, lastSeen
```

**Telemetry** (registry owns this; append-only JSONL when enabled)
```
event, ts, stationId, roomId, sessionId, mode
```

## 3) Service Relationships (ASCII map)

```
                     +----------------------+
  Web UI (Vercel) <--|  Render Registry API |--> data.json (stations/rooms)
        |            +----------------------+
        |                          ^
        |                          |
        v                          |
  HLS playback (Fly) <-------------+ token + ingest urls
        ^
        |
  WebRTC/RTMP ingest (Fly)

  EXE (optional) ----> Render Registry API (station updates)
  EXE (optional) ----> Fly ingest (audio layer)
```

## 4) Flow: Web Host (Anonymous)

1) UI calls Render: `POST /api/stations/web/begin` with mode=anon.
2) Render creates a **session** and returns:
   - signed token
   - ingest URLs (Fly base + token)
   - stationId = "anon"
3) UI uses WebRTC to send mic to Fly ingest.
4) Render marks station as broadcasting and exposes HLS URL.
5) UI + listeners play HLS from Fly.
6) Render expires session after 4 minutes, marks station idle.

## 5) Flow: Web Host (Creator)

1) UI calls Render with creator metadata (requires registry key if locked).
2) Render upserts a **Station** and returns token + ingest URLs.
3) UI sends audio to Fly ingest.
4) Render marks station broadcasting, stores session + streamUrl.

## 6) Flow: EXE Add-on

1) EXE gets stationId and token (creator flow or existing config).
2) EXE sends station updates to Render:
   - mood, energy, status, description, coverImage, etc.
3) EXE optionally publishes audio layer to Fly ingest (RTMP).
4) Render updates station metadata; UI reads it normally.

## 7) Efficiency + Lean Ops

- **Registry writes**: only on station upsert/heartbeat and session changes.
- **Listener counts**: in-memory map + TTL; no DB writes per listener event.
- **Rooms**: in-memory + occasional JSON save (current pattern).
- **Ingest server**: stateless, no DB.
- **UI**: pure client; can be cached CDN.

## 8) Failure Modes

- Fly ingest down: station shows broadcasting=false, UI shows playback error.
- Render down: UI shows registry offline; no station list updates.
- Vercel down: no UI, but ingest can still run (metadata stale).
- EXE down: station metadata becomes stale, but web host can continue.

## 9) Minimal Consistency Rules

- Render is the only service that assigns sessions and tokens.
- Fly never writes to registry; it only serves media.
- UI never writes to Fly; it only posts to Render and sends media to Fly.
- EXE posts only to Render (metadata) + Fly (audio).

## 10) Required Env Wiring

**Render (registry):**
- KEEGAN_INGEST_SECRET
- KEEGAN_INGEST_RTMP_BASE
- KEEGAN_INGEST_HLS_BASE
- KEEGAN_INGEST_WEBRTC_BASE
- ALLOWED_ORIGINS
- KEEGAN_REGISTRY_KEY (optional)

**Vercel (web UI):**
- VITE_REGISTRY_URL
- VITE_REGISTRY_KEY (if creator mode locked)

**Fly (ingest):**
- No registry secrets needed. Just MediaMTX.

---

# Open Questions (lean + ops)
1) Should we move registry storage from JSON to a real DB (SQLite/Postgres), or keep JSON for now?
2) Do you want EXE to be able to "claim" a station that was created on the web (pairing flow)?
3) Should anonymous broadcasts be recorded or always ephemeral only?

## Decisions (locked)
- **Registry storage**: keep JSON for now. Migrate to SQLite/Postgres when we cross ~50 active stations or ~10k listener events/day.
- **EXE pairing**: yes, allow EXE to claim a web-created station via a short pairing code.
- **Anonymous**: always ephemeral, no recording.

## EXE Pairing Flow (spec)

Goal: let a web-created station be claimed by a local EXE without sharing long-lived secrets.

### Flow
1) Web UI requests a short-lived pairing code for a station.
2) EXE enters the code and receives a station-bound token.
3) EXE can now update station metadata and (optionally) publish audio to Fly.

### Minimal endpoints
```
POST /api/stations/<id>/pairing/start
  -> { pairingCode, expiresAtMs }

POST /api/stations/pairing/claim
  body: { pairingCode, deviceName }
  -> { stationId, stationToken }
```

### Rules
- Pairing code expires in 5 minutes.
- Code is single-use.
- Station token is scoped to that station only.
- If `KEEGAN_REGISTRY_KEY` is enabled, pairing start requires it.

---

# Frequency Console UI Overhaul (Spec v2)

Goal: make the web console feel like a real radio control desk, not a generic dashboard. The UI should feel tactile, dense, and intentional. It should look hand-designed with a distinct palette, strong typography, and analog-inspired details.

## Design intent
- "Studio + signal lab": a dark control room with warm copper accents and cool ion-blue highlights.
- Prioritize readability for long sessions (low glare, soft gradients, clear labels).
- Signal energy should feel alive (subtle motion, animated status lights).

## Visual system
### Palette (hex)
- Ink: #0b0e13 (primary background)
- Charcoal: #121822 (panel background)
- Copper: #f2a44b (primary accent)
- Ion: #6ad9ff (secondary accent)
- Ember: #ff6b5b (error/alert)
- Cloud: #e8eef7 (primary text)
- Slate: #97a3b6 (muted text)

### Typography
- Display: "Unbounded" (headings, console title)
- Body: "Sora" (paragraphs, card content)
- Mono: "JetBrains Mono" (labels, codes, URLs)

### Texture + background
- Base gradient: vertical ink -> charcoal.
- Soft radial glows near corners (copper + ion).
- Thin grid overlay (very low opacity) to echo a control panel.

## Layout (desktop)
- Header: brand left, registry status and source selector right.
- Main: 2-column layout.
  - Left (wide): Regional Directory (stations + rooms tabs).
  - Right (stacked): Engine Core, Broadcast Desk, Stream Monitor.
- Panels: thicker borders, more contrast, and inner shadows to feel like hardware modules.

## Component treatments
- Station cards: frequency badge, status chip, listener count, and one-line description.
- Room cards: frequency badge + room tag, "Tune" action, listener pulse.
- Status chips: circle LED + label (online / offline / CORS / timeout).
- Input fields: dark inset, sharp focus ring in copper/ion.

## Motion
- Page load: slight upward fade (200-300ms).
- Registry status LED: slow pulse when checking.
- Background glow drift: 12-18s loop.

## Accessibility
- Minimum contrast ratio 4.5:1 for text and labels.
- Clear focus ring on all interactive elements.
- Avoid color-only status; always pair with label.

## Implementation notes
- Update `web/src/index.css` with new CSS variables + font imports.
- Refactor `web/src/App.tsx` layout and panels to match the new structure.
- Refine `web/src/components/StationDirectory.tsx` cards with the new visual system.
