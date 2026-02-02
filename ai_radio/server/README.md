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

## UI
Open http://localhost:8090/ to view the directory.

## Tests
```bash
python -m unittest server/tests/test_registry.py
```

## Ingest
See `server/ingest/README.md` for MediaMTX wiring.

