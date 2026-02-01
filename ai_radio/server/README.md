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
Set `ALLOWED_ORIGINS` (comma-separated) to allow custom web UI origins.

## API
- GET /api/stations
- GET /api/rooms
- POST /api/stations
- POST /api/stations/<id>/heartbeat
- POST /api/stations/<id>/listen
- GET /api/stations/<id>
- GET /api/rooms/<roomId>
- POST /api/rooms/<roomId>/presence
- GET /health

## UI
Open http://localhost:8090/ to view the directory.

## Ingest
See `server/ingest/README.md` for MediaMTX wiring.

