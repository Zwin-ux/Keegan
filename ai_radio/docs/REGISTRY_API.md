# Station Registry API

The station registry is a lightweight directory for regional discovery.

Base URL (default): http://localhost:8090

## Auth
If `KEEGAN_REGISTRY_KEY` is set, all POST endpoints require:
- `X-Api-Key: <key>` or
- `Authorization: Bearer <key>`

Telemetry is opt-in. Set `KEEGAN_TELEMETRY=1` on the registry to enable `/api/telemetry` logging.

## Endpoints
### GET /api/stations
Query params:
- region (optional)
- active=1 (optional)
Response fields include:
- `listenerCount` (active listeners in last 30s)

### GET /api/rooms
Query params:
- region (optional)
- appKey (optional)
- toneId (optional)
Response fields include:
- `listenerCount` (active listeners in last 30s)

### GET /api/seed
Returns the current global room seed (UTC) so clients do not rely on local clocks.
Response:
```
{ "seed": "2026-02-02", "tz": "UTC" }
```

### POST /api/stations
Upserts a station record. Used by the EXE heartbeat.
Example payload:
```
{
  "id": "st_123",
  "name": "Keegan Local",
  "region": "us-midwest",
  "frequency": 98.7,
  "description": "Local vibe engine broadcast",
  "streamUrl": "",
  "status": "live",
  "broadcasting": true,
  "mood": "focus_room",
  "energy": 0.55,
  "playing": true
}
```

### POST /api/stations/<id>/heartbeat
Keeps the station active without updating metadata.

### POST /api/stations/<id>/listen
Tracks listener sessions.
Body:
```
{
  "listenerId": "listener_123",
  "action": "join" // join | heartbeat | leave
}
```
Response:
```
{
  "listenerId": "listener_123",
  "listenerCount": 4
}
```

### GET /api/stations/<id>
Fetch a single station.

### GET /api/rooms/<roomId>
Fetch a single room.

### POST /api/rooms/<roomId>/presence
Tracks room presence.
Body:
```
{
  "listenerId": "listener_123",
  "action": "join", // join | heartbeat | leave
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
  "listenerId": "listener_123",
  "listenerCount": 4,
  "lastSeen": 1738420000000
}
```

### POST /api/telemetry
Accepts telemetry events (JSON) and appends to daily JSONL logs when enabled.
Response:
```
{ "ok": true, "stored": true }
```

### GET /health
Returns:
```
{
  "ok": true,
  "version": "0.2.0",
  "time": 1738420000000
}
```

## Directory UI
Open http://localhost:8090/ to view the minimal directory UI.

