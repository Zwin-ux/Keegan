# Local Bridge API

The EXE hosts a local HTTP server for UI and automation. Current code lives in `src/ui/web_server.*`.

## Base
- Host: localhost
- Port: 3000 (default)

## Auth
If `KEEGAN_BRIDGE_KEY` is set, all POST endpoints require:
- `X-Api-Key: <key>` or
- `Authorization: Bearer <key>`

Broadcast tokens are signed with `KEEGAN_BROADCAST_SECRET` (defaults to `KEEGAN_BRIDGE_KEY`, then `dev_secret`).
Tokens are bound to a single station ID and expire after ~10 minutes, so they cannot be reused across stations.

Ingest URL overrides:
- `KEEGAN_RTMP_URL` (default `rtmp://localhost/live`)
- `KEEGAN_HLS_URL` (default `http://localhost:8888/live`)
- `KEEGAN_WEBRTC_URL` (default `http://localhost:8889/live`)

## Implemented
### GET /api/state
Returns current mood, energy, and playing state.
Response example:
```
{
  "mood": "focus_room",
  "targetMood": "rain_cave",
  "energy": 0.55,
  "intensity": 0.7,
  "activity": 0.42,
  "idleSeconds": 3.1,
  "playing": true,
  "activeProcess": "code.exe",
  "updatedAtMs": 1738419200000
}
```

### POST /api/toggle
Toggles play/pause.
Response example:
```
{ "playing": "true" }
```

### POST /api/mood
Body: `{ "mood": "focus_room" }`
Sets the target mood and returns the latest state snapshot.

### GET /api/vibe
Returns a privacy-safe vibe vector for streaming metadata.

### GET /api/health
Basic health response.

### WebSocket (preferred)
WebSocket endpoint (default): `ws://localhost:3001/events`
Messages: JSON payload matching `/api/state`.
The WS server binds to `port + 1` relative to the HTTP server.
If `KEEGAN_BRIDGE_KEY` is set, pass `?token=<key>` or `X-Api-Key`/`Authorization` header.

### GET /api/events (deprecated)
Returns 410 with a hint to use WebSocket instead.

### POST /api/broadcast/start
Starts a live broadcast session. Requires token in body, `Authorization: Bearer`, or `X-Broadcast-Token`.
Optional body field: `streamUrl` to update the station stream URL.
Returns `sessionId`, `startedAtMs`.

### POST /api/broadcast/stop
Stops the broadcast session. Requires token in body, `Authorization: Bearer`, or `X-Broadcast-Token`.

### POST /api/broadcast/token
Returns a short-lived token for ingest auth (`expiresAtMs`, `expiresInMs`).

### GET /api/broadcast/ingest
Returns ingest URLs for WebRTC/RTMP/HLS. Requires token in `Authorization` or `X-Broadcast-Token`.
The token is used as the stream key for RTMP/HLS.

### GET /api/broadcast/status
Returns current session state (broadcasting, sessionId, timestamps, streamUrl). Requires auth if `KEEGAN_BRIDGE_KEY` is set.

### POST /api/pairing/start
Starts pairing for the current station by requesting a code from the registry.
Requires registry URL to be configured and `KEEGAN_REGISTRY_KEY` if enabled.
Response:
```
{ "pairingCode": "AB12CD", "expiresAtMs": 1738420000000 }
```

### POST /api/pairing/claim
Claims a pairing code and stores a station token locally (used for registry updates).
Body:
```
{ "pairingCode": "AB12CD" }
```
Response:
```
{ "ok": true, "stationId": "st_123", "expiresAtMs": 1738420000000 }
```

