import base64
import hashlib
import hmac
import json
import os
import random
import time
import threading
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

DATA_DIR = os.path.join(os.path.dirname(__file__), 'data')
DATA_FILE = os.path.join(DATA_DIR, 'stations.json')
ROOMS_FILE = os.path.join(DATA_DIR, 'rooms.json')
STATIC_DIR = os.path.join(os.path.dirname(__file__), 'static')

DEFAULT_REGION = os.environ.get('KEEGAN_DEFAULT_REGION', 'us-midwest')
REGISTRY_KEY = os.environ.get('KEEGAN_REGISTRY_KEY')
REGISTRY_VERSION = os.environ.get('REGISTRY_VERSION', '0.3.0')

INGEST_SECRET = (
    os.environ.get('KEEGAN_INGEST_SECRET')
    or os.environ.get('KEEGAN_BROADCAST_SECRET')
    or os.environ.get('KEEGAN_REGISTRY_KEY')
    or 'dev-secret'
)
INGEST_RTMP_BASE = os.environ.get('KEEGAN_INGEST_RTMP_BASE', 'rtmp://localhost/live')
INGEST_HLS_BASE = os.environ.get('KEEGAN_INGEST_HLS_BASE', 'http://localhost:8888/live')
INGEST_WEBRTC_BASE = os.environ.get('KEEGAN_INGEST_WEBRTC_BASE', 'http://localhost:8889/live')

ANON_SESSION_MS = int(os.environ.get('KEEGAN_ANON_SESSION_MS', str(4 * 60 * 1000)))
ANON_COOLDOWN_MS = int(os.environ.get('KEEGAN_ANON_COOLDOWN_MS', str(10 * 60 * 1000)))
CREATOR_SESSION_MS = int(os.environ.get('KEEGAN_CREATOR_SESSION_MS', str(12 * 60 * 60 * 1000)))

DEFAULT_ALLOWED_ORIGINS = [
    'http://localhost:3000',
    'http://127.0.0.1:3000',
    'http://localhost:5173',
    'http://127.0.0.1:5173',
    'https://keegan-khaki.vercel.app',
]


def parse_allowed_origins(raw):
    if not raw:
        return DEFAULT_ALLOWED_ORIGINS
    raw = raw.strip()
    if raw == '*':
        return ['*']
    return [item.strip() for item in raw.split(',') if item.strip()]


ALLOWED_ORIGINS = parse_allowed_origins(os.environ.get('ALLOWED_ORIGINS', ''))


def now_ms():
    return int(time.time() * 1000)

def current_seed():
    return time.strftime('%Y-%m-%d', time.gmtime())

def telemetry_enabled():
    raw = os.environ.get('KEEGAN_TELEMETRY', '')
    return raw.lower() in ('1', 'true', 'yes', 'on')


def telemetry_path():
    stamp = time.strftime('%Y-%m-%d')
    return os.path.join(DATA_DIR, f"telemetry-{stamp}.jsonl")


def append_telemetry(event):
    if not telemetry_enabled():
        return False
    if not isinstance(event, dict):
        return False
    if 'ts' not in event:
        event['ts'] = now_ms()
    os.makedirs(DATA_DIR, exist_ok=True)
    with open(telemetry_path(), 'a', encoding='utf-8') as f:
        f.write(json.dumps(event) + '\n')
    return True


def _b64url_encode(raw):
    return base64.urlsafe_b64encode(raw).rstrip(b'=').decode('utf-8')


def _b64url_decode(raw):
    padding = '=' * (-len(raw) % 4)
    return base64.urlsafe_b64decode(raw + padding)


def _sign_message(message):
    digest = hmac.new(INGEST_SECRET.encode('utf-8'), message.encode('utf-8'), hashlib.sha256).digest()
    return _b64url_encode(digest)


def make_ingest_token(payload):
    raw = json.dumps(payload, separators=(',', ':')).encode('utf-8')
    body = _b64url_encode(raw)
    signature = _sign_message(body)
    return f"{body}.{signature}"


def verify_ingest_token(token):
    if not token or '.' not in token:
        return None
    body, signature = token.split('.', 1)
    expected = _sign_message(body)
    if not hmac.compare_digest(signature, expected):
        return None
    try:
        payload = json.loads(_b64url_decode(body).decode('utf-8'))
    except Exception:
        return None
    if payload.get('exp') and payload['exp'] < now_ms():
        return None
    return payload


class StationStore:
    def __init__(self):
        self.stations = {}
        self.listeners = {}
        self.lock = threading.Lock()
        self.load()

    def load(self):
        try:
            if os.path.exists(DATA_FILE):
                with open(DATA_FILE, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                for station in data.get('stations', []):
                    if 'id' in station:
                        self.stations[station['id']] = station
        except Exception:
            self.stations = {}

    def save(self):
        os.makedirs(DATA_DIR, exist_ok=True)
        data = {'stations': list(self.stations.values())}
        with open(DATA_FILE, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2)

    def _prune_listeners(self, station_id, ttl_ms=30000):
        listeners = self.listeners.get(station_id, {})
        cutoff = now_ms() - ttl_ms
        to_remove = [lid for lid, seen in listeners.items() if seen < cutoff]
        for lid in to_remove:
            listeners.pop(lid, None)
        if listeners:
            self.listeners[station_id] = listeners
        elif station_id in self.listeners:
            self.listeners.pop(station_id, None)
        return len(listeners)

    def list(self, region=None, active_only=False, active_window_ms=5 * 60 * 1000):
        with self.lock:
            items = list(self.stations.values())
            if region:
                items = [s for s in items if s.get('region') == region]
            if active_only:
                cutoff = now_ms() - active_window_ms
                items = [s for s in items if s.get('lastSeen', 0) >= cutoff]
            enriched = []
            for s in items:
                copy = dict(s)
                copy['listenerCount'] = self._prune_listeners(s.get('id', ''))
                enriched.append(copy)
            return sorted(enriched, key=lambda s: s.get('updatedAt', 0), reverse=True)

    def get(self, station_id):
        with self.lock:
            station = self.stations.get(station_id)
            if not station:
                return None
            copy = dict(station)
            copy['listenerCount'] = self._prune_listeners(station_id)
            return copy

    def upsert(self, payload):
        with self.lock:
            station_id = payload.get('id')
            if not station_id:
                station_id = f"st_{int(time.time())}_{random.randint(1000, 9999)}"

            existing = self.stations.get(station_id)
            created_at = existing.get('createdAt') if existing else now_ms()

            merged = {
                **(existing or {}),
                **payload,
                'id': station_id,
                'createdAt': created_at,
                'updatedAt': now_ms(),
                'lastSeen': now_ms(),
            }

            if 'region' not in merged or not merged['region']:
                merged['region'] = DEFAULT_REGION
            if 'status' not in merged or not merged['status']:
                merged['status'] = 'live'

            self.stations[station_id] = merged
            self.save()
            merged = dict(merged)
            merged['listenerCount'] = self._prune_listeners(station_id)
            return merged

    def heartbeat(self, station_id):
        with self.lock:
            station = self.stations.get(station_id)
            if not station:
                return None
            station['lastSeen'] = now_ms()
            station['status'] = station.get('status', 'live')
            station['updatedAt'] = now_ms()
            self.save()
            copy = dict(station)
            copy['listenerCount'] = self._prune_listeners(station_id)
            return copy

    def listen(self, station_id, listener_id, action='join'):
        with self.lock:
            station = self.stations.get(station_id)
            if not station:
                return None
            if not listener_id:
                listener_id = f"listener_{int(time.time())}_{random.randint(1000, 9999)}"
            listeners = self.listeners.get(station_id, {})
            if action == 'leave':
                listeners.pop(listener_id, None)
            else:
                listeners[listener_id] = now_ms()
            self.listeners[station_id] = listeners
            count = self._prune_listeners(station_id)
            return {'listenerId': listener_id, 'listenerCount': count}

    def set_broadcast(self, station_id, broadcasting, stream_url=None, session_id=None, mode=None, metadata=None):
        with self.lock:
            station = self.stations.get(station_id, {})
            if not station:
                station = {'id': station_id}
            if metadata:
                station.update(metadata)
            station['broadcasting'] = bool(broadcasting)
            station['status'] = 'live' if broadcasting else 'idle'
            station['updatedAt'] = now_ms()
            station['lastSeen'] = now_ms()
            if broadcasting:
                if stream_url:
                    station['streamUrl'] = stream_url
                if session_id:
                    station['broadcastSessionId'] = session_id
                if mode:
                    station['broadcastMode'] = mode
                station['broadcastStartedAtMs'] = now_ms()
            else:
                station['broadcastSessionId'] = None
                station['broadcastMode'] = None
                station['broadcastEndedAtMs'] = now_ms()
            self.stations[station_id] = station
            self.save()
            copy = dict(station)
            copy['listenerCount'] = self._prune_listeners(station_id)
            return copy


STORE = StationStore()


class RoomStore:
    def __init__(self):
        self.rooms = {}
        self.listeners = {}
        self.lock = threading.Lock()
        self.load()

    def load(self):
        try:
            if os.path.exists(ROOMS_FILE):
                with open(ROOMS_FILE, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                for room in data.get('rooms', []):
                    if 'roomId' in room:
                        self.rooms[room['roomId']] = room
        except Exception:
            self.rooms = {}

    def save(self):
        os.makedirs(DATA_DIR, exist_ok=True)
        data = {'rooms': list(self.rooms.values())}
        with open(ROOMS_FILE, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2)

    def _prune_listeners(self, room_id, ttl_ms=30000):
        listeners = self.listeners.get(room_id, {})
        cutoff = now_ms() - ttl_ms
        to_remove = [lid for lid, seen in listeners.items() if seen < cutoff]
        for lid in to_remove:
            listeners.pop(lid, None)
        if listeners:
            self.listeners[room_id] = listeners
        elif room_id in self.listeners:
            self.listeners.pop(room_id, None)
        return len(listeners)

    def _assign_frequency(self, room_id):
        import hashlib
        min_freq = 87.0
        max_freq = 108.0
        step = 0.1
        slots = int(round((max_freq - min_freq) / step)) + 1
        digest = hashlib.sha1(room_id.encode('utf-8')).hexdigest()
        index = int(digest[:8], 16) % slots
        used = {}
        for room in self.rooms.values():
            freq = room.get('frequency')
            if isinstance(freq, (int, float)) and freq > 0:
                used[round(float(freq), 1)] = room.get('roomId')
        for offset in range(slots):
            freq = round(min_freq + ((index + offset) % slots) * step, 1)
            if freq not in used or used[freq] == room_id:
                return freq
        return round(min_freq, 1)

    def list(self, region=None, app_key=None, tone_id=None):
        with self.lock:
            items = list(self.rooms.values())
            if region:
                items = [r for r in items if r.get('region') == region]
            if app_key:
                items = [r for r in items if r.get('appKey') == app_key]
            if tone_id:
                items = [r for r in items if r.get('toneId') == tone_id]
            enriched = []
            for r in items:
                copy = dict(r)
                copy['listenerCount'] = self._prune_listeners(r.get('roomId', ''))
                enriched.append(copy)
            return sorted(enriched, key=lambda r: r.get('lastSeen', 0), reverse=True)

    def get(self, room_id):
        with self.lock:
            room = self.rooms.get(room_id)
            if not room:
                return None
            copy = dict(room)
            copy['listenerCount'] = self._prune_listeners(room_id)
            return copy

    def presence(self, room_id, payload):
        with self.lock:
            if not room_id:
                return None
            room = self.rooms.get(room_id, {})
            room['roomId'] = room_id
            room['region'] = payload.get('region') or room.get('region') or DEFAULT_REGION
            room['appKey'] = payload.get('appKey') or room.get('appKey') or 'unknown'
            room['toneId'] = payload.get('toneId') or room.get('toneId') or 'default'
            if not room.get('frequency'):
                room['frequency'] = self._assign_frequency(room_id)
            room['lastSeen'] = now_ms()
            self.rooms[room_id] = room
            self.save()

            listener_id = payload.get('listenerId')
            if not listener_id:
                listener_id = f"listener_{int(time.time())}_{random.randint(1000, 9999)}"
            action = payload.get('action', 'join')
            listeners = self.listeners.get(room_id, {})
            if action == 'leave':
                listeners.pop(listener_id, None)
            else:
                listeners[listener_id] = now_ms()
            self.listeners[room_id] = listeners
            count = self._prune_listeners(room_id)
            return {
                'roomId': room_id,
                'listenerId': listener_id,
                'listenerCount': count,
                'lastSeen': room.get('lastSeen', 0),
            }


ROOMS = RoomStore()


class IngestSessionStore:
    def __init__(self):
        self.sessions = {}
        self.station_sessions = {}
        self.cooldowns = {}
        self.lock = threading.Lock()

    def _ingest_urls(self, token):
        rtmp = f"{INGEST_RTMP_BASE.rstrip('/')}/{token}"
        hls = f"{INGEST_HLS_BASE.rstrip('/')}/{token}/index.m3u8"
        webrtc = f"{INGEST_WEBRTC_BASE.rstrip('/')}/{token}"
        return {
            'rtmpUrl': rtmp,
            'hlsUrl': hls,
            'webrtcUrl': webrtc,
        }

    def _active_session(self, station_id):
        session_id = self.station_sessions.get(station_id)
        if not session_id:
            return None
        return self.sessions.get(session_id)

    def _cleanup_expired(self):
        now = now_ms()
        expired = [sid for sid, sess in self.sessions.items() if sess.get('endsAtMs') and sess['endsAtMs'] <= now]
        for sid in expired:
            self._end_session_locked(sid, reason='expired')

    def _end_session_locked(self, session_id, reason='stopped'):
        session = self.sessions.pop(session_id, None)
        if not session:
            return None
        station_id = session['stationId']
        if self.station_sessions.get(station_id) == session_id:
            self.station_sessions.pop(station_id, None)
        if session.get('mode') == 'anon':
            client_id = session.get('clientId')
            if client_id:
                self.cooldowns[client_id] = now_ms()
        STORE.set_broadcast(station_id, False, metadata={'broadcastStopReason': reason})
        return session

    def begin(self, station_id, mode, client_id, duration_ms, metadata, allow_replace=False):
        with self.lock:
            self._cleanup_expired()
            existing = self._active_session(station_id)
            if existing and not allow_replace:
                return None, {'error': 'already_live', 'status': 409}
            if mode == 'anon':
                last_end = self.cooldowns.get(client_id)
                if last_end and now_ms() - last_end < ANON_COOLDOWN_MS:
                    return None, {'error': 'cooldown', 'status': 429}
            session_id = f"sess_{uuid.uuid4().hex[:12]}"
            starts_at = now_ms()
            ends_at = starts_at + duration_ms if duration_ms else None
            payload = {
                'stationId': station_id,
                'sessionId': session_id,
                'mode': mode,
                'exp': ends_at or (starts_at + CREATOR_SESSION_MS),
            }
            token = make_ingest_token(payload)
            session = {
                'stationId': station_id,
                'sessionId': session_id,
                'mode': mode,
                'clientId': client_id,
                'startedAtMs': starts_at,
                'endsAtMs': ends_at,
                'token': token,
            }
            self.sessions[session_id] = session
            self.station_sessions[station_id] = session_id
            ingest = self._ingest_urls(token)
            STORE.set_broadcast(station_id, True, stream_url=ingest['hlsUrl'], session_id=session_id, mode=mode, metadata=metadata)
            return {
                'session': session,
                'ingest': ingest,
                'stationId': station_id,
                'token': token,
            }, None

    def stop(self, token=None, station_id=None, session_id=None):
        with self.lock:
            self._cleanup_expired()
            resolved = None
            if token:
                payload = verify_ingest_token(token)
                if not payload:
                    return None, {'error': 'invalid_token', 'status': 401}
                station_id = payload.get('stationId')
                session_id = payload.get('sessionId')
            if session_id:
                resolved = session_id
            elif station_id:
                active = self._active_session(station_id)
                resolved = active['sessionId'] if active else None
            if not resolved:
                return None, {'error': 'not_found', 'status': 404}
            session = self._end_session_locked(resolved, reason='stopped')
            return session, None

    def status(self, station_id):
        with self.lock:
            self._cleanup_expired()
            session = self._active_session(station_id)
            if not session:
                return None
            return dict(session)


SESSIONS = IngestSessionStore()


class RegistryHandler(BaseHTTPRequestHandler):
    def _origin_allowed(self, origin):
        if '*' in ALLOWED_ORIGINS:
            return True
        if origin in ALLOWED_ORIGINS:
            return True
        if not origin:
            return False
        try:
            parsed = urlparse(origin)
            host = parsed.hostname or ''
        except Exception:
            host = ''
        for pattern in ALLOWED_ORIGINS:
            if '*.' not in pattern:
                continue
            if '://' in pattern:
                try:
                    pattern_host = urlparse(pattern).hostname or ''
                except Exception:
                    pattern_host = ''
            else:
                pattern_host = pattern
            if pattern_host.startswith('*.') and host.endswith(pattern_host[1:]):
                return True
        return False

    def _cors_origin(self):
        origin = self.headers.get('Origin')
        if '*' in ALLOWED_ORIGINS:
            return '*'
        if origin and self._origin_allowed(origin):
            return origin
        return ''

    def _send_cors(self):
        origin = self._cors_origin()
        if origin:
            self.send_header('Access-Control-Allow-Origin', origin)
            if origin != '*':
                self.send_header('Vary', 'Origin')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Api-Key, X-Client-Id, X-Session-Id')

    def _send_json(self, status, payload):
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self._send_cors()
        self.end_headers()
        self.wfile.write(json.dumps(payload).encode('utf-8'))

    def _authorized(self):
        if not REGISTRY_KEY:
            return True
        header_key = self.headers.get('X-Api-Key')
        if header_key == REGISTRY_KEY:
            return True
        auth = self.headers.get('Authorization', '')
        if auth.startswith('Bearer '):
            return auth.split('Bearer ', 1)[1] == REGISTRY_KEY
        return False

    def do_OPTIONS(self):
        origin = self.headers.get('Origin')
        if origin and not self._origin_allowed(origin):
            self.send_response(403)
            self.end_headers()
            return
        self.send_response(204)
        self._send_cors()
        self.end_headers()

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == '/api/stations':
            params = parse_qs(parsed.query)
            region = params.get('region', [None])[0]
            active_only = params.get('active', ['0'])[0] == '1'
            stations = STORE.list(region=region, active_only=active_only)
            self._send_json(200, {'stations': stations})
            return

        if parsed.path == '/api/rooms':
            params = parse_qs(parsed.query)
            region = params.get('region', [None])[0]
            app_key = params.get('appKey', [None])[0]
            tone_id = params.get('toneId', [None])[0]
            rooms = ROOMS.list(region=region, app_key=app_key, tone_id=tone_id)
            self._send_json(200, {'rooms': rooms})
            return

        if parsed.path == '/api/seed':
            self._send_json(200, {'seed': current_seed(), 'tz': 'UTC'})
            return

        if parsed.path.startswith('/api/stations/'):
            station_id = parsed.path.split('/api/stations/')[1]
            if station_id.endswith('/status'):
                station_id = station_id.split('/status')[0]
                session = SESSIONS.status(station_id)
                station = STORE.get(station_id)
                if not station:
                    self._send_json(404, {'error': 'not_found'})
                    return
                payload = {
                    'stationId': station_id,
                    'broadcasting': station.get('broadcasting', False),
                    'startedAtMs': session.get('startedAtMs') if session else station.get('broadcastStartedAtMs'),
                    'endsAtMs': session.get('endsAtMs') if session else None,
                    'sessionId': session.get('sessionId') if session else None,
                }
                self._send_json(200, payload)
                return
            station = STORE.get(station_id)
            if not station:
                self._send_json(404, {'error': 'not_found'})
                return
            self._send_json(200, station)
            return

        if parsed.path.startswith('/api/rooms/'):
            room_id = parsed.path.split('/api/rooms/')[1]
            room = ROOMS.get(room_id)
            if not room:
                self._send_json(404, {'error': 'not_found'})
                return
            self._send_json(200, room)
            return

        if parsed.path == '/health':
            self._send_json(200, {'ok': True, 'version': REGISTRY_VERSION, 'time': now_ms()})
            return

        self._serve_static(parsed.path)

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path == '/api/stations/web/begin':
            length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(length) if length > 0 else b'{}'
            try:
                payload = json.loads(body.decode('utf-8'))
            except Exception:
                self._send_json(400, {'error': 'invalid_json'})
                return
            mode = payload.get('mode', 'creator')
            station_payload = payload.get('station') or {}
            station_id = station_payload.get('id')
            if station_id == 'anon' or mode == 'anon':
                mode = 'anon'
                station_id = 'anon'
                station_payload.setdefault('name', 'Anonymous Frequency')
                station_payload.setdefault('description', 'Open mic drop (4 minutes).')
                station_payload.setdefault('region', DEFAULT_REGION)
            else:
                mode = 'creator'
                if REGISTRY_KEY and not self._authorized():
                    self._send_json(401, {'error': 'unauthorized'})
                    return

            client_id = (
                self.headers.get('X-Client-Id')
                or payload.get('clientId')
                or self.headers.get('X-Session-Id')
                or self.client_address[0]
            )

            if mode == 'creator':
                station = STORE.upsert(station_payload)
                station_id = station.get('id')
            else:
                station = STORE.upsert({**station_payload, 'id': station_id, 'status': 'live'})

            duration_ms = ANON_SESSION_MS if mode == 'anon' else CREATOR_SESSION_MS
            result, error = SESSIONS.begin(
                station_id=station_id,
                mode=mode,
                client_id=client_id,
                duration_ms=duration_ms,
                metadata={
                    'name': station.get('name'),
                    'description': station.get('description'),
                    'coverImage': station.get('coverImage'),
                    'region': station.get('region'),
                },
            )
            if error:
                self._send_json(error.get('status', 400), error)
                return

            session = result['session']
            ingest = result['ingest']
            append_telemetry({
                'event': 'web_broadcast_start',
                'stationId': station_id,
                'mode': mode,
                'sessionId': session.get('sessionId'),
            })
            self._send_json(200, {
                'stationId': result['stationId'],
                'sessionId': session.get('sessionId'),
                'token': result['token'],
                'expiresAtMs': session.get('endsAtMs') or session.get('startedAtMs'),
                'ingest': ingest,
                'station': STORE.get(station_id),
            })
            return

        if parsed.path == '/api/stations/web/stop':
            length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(length) if length > 0 else b'{}'
            try:
                payload = json.loads(body.decode('utf-8'))
            except Exception:
                payload = {}
            token = payload.get('token') or self.headers.get('X-Broadcast-Token')
            station_id = payload.get('stationId')
            session_id = payload.get('sessionId')
            session, error = SESSIONS.stop(token=token, station_id=station_id, session_id=session_id)
            if error:
                self._send_json(error.get('status', 400), error)
                return
            append_telemetry({
                'event': 'web_broadcast_stop',
                'stationId': session.get('stationId') if session else station_id,
                'sessionId': session.get('sessionId') if session else session_id,
            })
            self._send_json(200, {'ok': True, 'sessionId': session.get('sessionId') if session else None})
            return

        if parsed.path == '/api/stations':
            if not self._authorized():
                self._send_json(401, {'error': 'unauthorized'})
                return
            length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(length) if length > 0 else b'{}'
            try:
                payload = json.loads(body.decode('utf-8'))
            except Exception:
                self._send_json(400, {'error': 'invalid_json'})
                return
            station = STORE.upsert(payload)
            append_telemetry({
                'event': 'station_update',
                'stationId': station.get('id'),
                'region': station.get('region'),
                'status': station.get('status'),
                'broadcasting': station.get('broadcasting'),
            })
            self._send_json(200, station)
            return

        if parsed.path.startswith('/api/stations/') and parsed.path.endswith('/heartbeat'):
            if not self._authorized():
                self._send_json(401, {'error': 'unauthorized'})
                return
            station_id = parsed.path.split('/api/stations/')[1].split('/heartbeat')[0]
            station = STORE.heartbeat(station_id)
            if not station:
                self._send_json(404, {'error': 'not_found'})
                return
            self._send_json(200, station)
            return

        if parsed.path.startswith('/api/stations/') and parsed.path.endswith('/listen'):
            if not self._authorized():
                self._send_json(401, {'error': 'unauthorized'})
                return
            station_id = parsed.path.split('/api/stations/')[1].split('/listen')[0]
            length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(length) if length > 0 else b'{}'
            try:
                payload = json.loads(body.decode('utf-8'))
            except Exception:
                payload = {}
            listener_id = payload.get('listenerId')
            action = payload.get('action', 'join')
            result = STORE.listen(station_id, listener_id, action)
            if not result:
                self._send_json(404, {'error': 'not_found'})
                return
            if action in ('join', 'leave'):
                append_telemetry({
                    'event': 'listener_' + action,
                    'stationId': station_id,
                    'listenerId': result.get('listenerId'),
                })
            self._send_json(200, result)
            return

        if parsed.path.startswith('/api/rooms/') and parsed.path.endswith('/presence'):
            if not self._authorized():
                self._send_json(401, {'error': 'unauthorized'})
                return
            room_id = parsed.path.split('/api/rooms/')[1].split('/presence')[0]
            length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(length) if length > 0 else b'{}'
            try:
                payload = json.loads(body.decode('utf-8'))
            except Exception:
                payload = {}
            result = ROOMS.presence(room_id, payload)
            if not result:
                self._send_json(404, {'error': 'not_found'})
                return
            append_telemetry({
                'event': 'room_presence',
                'roomId': room_id,
                'region': payload.get('region'),
                'appKey': payload.get('appKey'),
                'toneId': payload.get('toneId'),
                'action': payload.get('action', 'join'),
                'listenerId': result.get('listenerId'),
            })
            self._send_json(200, result)
            return

        if parsed.path == '/api/telemetry':
            length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(length) if length > 0 else b'{}'
            try:
                payload = json.loads(body.decode('utf-8'))
            except Exception:
                self._send_json(400, {'error': 'invalid_json'})
                return
            stored = append_telemetry(payload)
            self._send_json(200, {'ok': True, 'stored': stored})
            return

        self._send_json(404, {'error': 'not_found'})

    def _serve_static(self, path):
        if path == '/':
            path = '/index.html'
        safe_path = path.lstrip('/')
        file_path = os.path.join(STATIC_DIR, safe_path)
        if not os.path.abspath(file_path).startswith(os.path.abspath(STATIC_DIR)):
            self.send_response(403)
            self.end_headers()
            return
        if not os.path.exists(file_path):
            self.send_response(404)
            self.end_headers()
            return

        if file_path.endswith('.html'):
            content_type = 'text/html'
        elif file_path.endswith('.css'):
            content_type = 'text/css'
        elif file_path.endswith('.js'):
            content_type = 'application/javascript'
        else:
            content_type = 'application/octet-stream'

        with open(file_path, 'rb') as f:
            content = f.read()

        self.send_response(200)
        self.send_header('Content-Type', content_type)
        self._send_cors()
        self.end_headers()
        self.wfile.write(content)

    def log_message(self, format, *args):
        print(f"[registry] {format % args}")


def run(port=8090):
    server = ThreadingHTTPServer(('0.0.0.0', port), RegistryHandler)
    print(f"Keegan registry server running on :{port}")
    server.serve_forever()


if __name__ == '__main__':
    port = int(os.environ.get('PORT', 8090))
    run(port=port)
