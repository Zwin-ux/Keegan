# MediaMTX Ingest (Fly.io)

This directory deploys MediaMTX as the public ingest server for WebRTC/RTMP/HLS.

## Deploy on Fly
1) Install flyctl and login.
2) From repo root:
```
cd ai_radio/ingest
fly launch
```
When prompted:
- App name: `keegan-ingest` (or your choice)
- Region: pick your closest
- Do **not** create a Postgres DB

3) Deploy:
```
fly deploy
```

## Ports
- RTMP: 1935 (tcp)
- HLS: 80/443 (tcp -> 8888)
- WebRTC signaling: 8889 (tcp)
- WebRTC UDP: 8000 (udp)

## Wire the registry (Render env)
Set these on your Render registry service:
```
KEEGAN_INGEST_SECRET=your-strong-secret
KEEGAN_INGEST_RTMP_BASE=rtmp://keegan-ingest.fly.dev/live
KEEGAN_INGEST_HLS_BASE=https://keegan-ingest.fly.dev/live
KEEGAN_INGEST_WEBRTC_BASE=https://keegan-ingest.fly.dev/live
```

If you use a custom domain for Fly, swap in that domain.
