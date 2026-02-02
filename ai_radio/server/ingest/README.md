# Ingest Wiring (MediaMTX)

This project expects an external media server to handle RTMP/WebRTC/HLS.
We recommend MediaMTX for local development and Fly.io for hosted ingest.

## Quick start
1) Download MediaMTX from: https://github.com/bluenviron/mediamtx/releases
2) Copy `mediamtx.yml` next to the binary.
3) Run:
```
mediamtx.exe mediamtx.yml
```

## Hosted ingest (Fly)
Use the deployable config in `ai_radio/ingest` and follow `ai_radio/ingest/README.md`.

## Expected URLs
- RTMP: rtmp://localhost/live/<token>
- HLS:  http://localhost:8888/live/<token>/index.m3u8
- WebRTC (playback): http://localhost:8889/live/<token>

The EXE bridge returns these URLs in `/api/broadcast/ingest`.

