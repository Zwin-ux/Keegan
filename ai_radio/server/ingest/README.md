# Ingest Wiring (MediaMTX)

This project expects an external media server to handle RTMP/WebRTC/HLS.
We recommend MediaMTX for local development.

## Quick start
1) Download MediaMTX from: https://github.com/bluenviron/mediamtx/releases
2) Copy `mediamtx.yml` next to the binary.
3) Run:
```
mediamtx.exe mediamtx.yml
```

## Expected URLs
- RTMP: rtmp://localhost/live/<token>
- HLS:  http://localhost:8888/live/<token>/index.m3u8
- WebRTC (playback): http://localhost:8889/live/<token>

The EXE bridge returns these URLs in `/api/broadcast/ingest`.

