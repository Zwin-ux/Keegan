# Keegan Station Registry

Minimal station registry service with a static directory UI.

## Run
```bash
cd ai_radio/server
python registry_server.py
```

Default port: 8090

## Auth
Set `KEEGAN_REGISTRY_KEY` to require API keys for POST requests.
Set `ALLOWED_ORIGINS` (comma-separated) to allow custom web UI origins. Wildcards like `*.vercel.app` are supported.
Set `KEEGAN_TELEMETRY=1` to enable `/api/telemetry` logging (writes JSONL to `server/data`).
Set `KEEGAN_INGEST_SECRET` to sign web-host ingest tokens.
Set `KEEGAN_INGEST_RTMP_BASE`, `KEEGAN_INGEST_HLS_BASE`, `KEEGAN_INGEST_WEBRTC_BASE` for hosted ingest URLs.
Anonymous mode tuning: `KEEGAN_ANON_SESSION_MS` (default 240000), `KEEGAN_ANON_COOLDOWN_MS` (default 600000).
Pairing flow: `KEEGAN_PAIRING_TTL_MS` (default 300000), `KEEGAN_STATION_TOKEN_MS` (default 2592000000).

## API
- GET /api/stations
- GET /api/rooms
- GET /api/seed
- POST /api/stations
- POST /api/stations/<id>/heartbeat
- POST /api/stations/<id>/listen
- GET /api/stations/<id>
- GET /api/rooms/<roomId>
- POST /api/rooms/<roomId>/presence
- POST /api/telemetry
- GET /health
- POST /api/stations/web/begin
- POST /api/stations/web/stop
- GET /api/stations/<id>/status
- POST /api/stations/<id>/pairing/start
- POST /api/stations/pairing/claim

## UI
Open http://localhost:8090/ to view the directory.

## Tests
```bash
python -m unittest server/tests/test_registry.py
```

## Ingest
See `server/ingest/README.md` for MediaMTX wiring.

