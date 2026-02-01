# Station Hosting Spec

## Goal
Make Keegan a place where anyone can host a radio station, run their own vibe engine, and appear in the shared directory with live status, listener counts, and secure ingest.

## Roles
- Station host: runs the EXE (engine) + ingest server and optionally exposes a public stream.
- Registry operator: runs the station directory service for a region.
- Listener client: web/mobile app that browses the registry and plays streams.

## Core components
- Keegan EXE (engine):
  - Generates the audio/vibe locally and exposes the Local Bridge API.
  - Registers station metadata to a registry server.
  - Issues signed ingest tokens (per-station).
- Ingest server (MediaMTX default):
  - Accepts RTMP ingest with token as stream key.
  - Serves HLS + WebRTC playback.
- Registry server:
  - Stores station metadata + live status.
  - Tracks listener counts via heartbeat.
- Web UI:
  - Lists stations by region + status.
  - Plays HLS streams and shows health metrics.
  - Provides a local ingest control panel for the host.

## Station identity + security
- Station ID:
  - Generated once and cached locally (`cache/station_id.txt`) if not set in `config/station.json`.
  - Included in signed ingest tokens so they cannot be reused across stations.
- Secrets:
  - `KEEGAN_BRIDGE_KEY` protects local control endpoints.
  - `KEEGAN_BROADCAST_SECRET` signs ingest tokens (defaults to bridge key).
  - `KEEGAN_REGISTRY_KEY` protects registry writes.

## Hosting flows
### 1) Register station
1. Host configures station metadata in `config/station.json` or env overrides.
2. EXE posts metadata + status to the registry every 15 seconds.
3. Registry assigns or confirms station ID.

### 2) Start broadcast session
1. Host requests `/api/broadcast/token`.
2. Token returned is short-lived and station-bound.
3. Host starts `/api/broadcast/start` to mark session live (optional stream URL override).

### 3) Ingest audio
- RTMP ingest URL: `rtmp://<ingest-host>/live/<token>`
- HLS playback URL: `http://<ingest-host>:8888/live/<token>/index.m3u8`
- WebRTC playback URL: `http://<ingest-host>:8889/live/<token>`

### 4) Listener experience
- Web client polls registry for stations.
- When a station is selected, HLS playback begins.
- Client posts `/api/stations/<id>/listen` heartbeats for counts.

## Host modes
- Local-only: run registry + ingest on localhost, share in a private network.
- Regional: host registry + ingest in a region, accept community stations.
- Community: multiple stations with a shared registry and delegated keys.

## Modding + personalization
- Station hosts can install frequency packs (`mods/`) to change moods/stems/curves.
- Provide station-specific metadata: name, region, description, frequency.
- Future: allow custom visuals, EQ presets, and station intros.

## Operational checklist (MVP)
- Run registry: `python server/registry_server.py`
- Run ingest: MediaMTX with `server/ingest/mediamtx.yml`
- Run EXE: set `KEEGAN_REGISTRY_URL` + keys
- Use the Web UI ingest panel to generate token + start broadcast
- Push stream via RTMP using token as stream key

## Future extensions
- Hosted station onboarding + key management.
- Regional directory federation and discovery.
- Moderation/flagging for public listings.
- Mobile app with push notifications for station schedules.
