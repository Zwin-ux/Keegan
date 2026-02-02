import json
import os
import random
import time
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

DATA_DIR = os.path.join(os.path.dirname(__file__), 'data')
DATA_FILE = os.path.join(DATA_DIR, 'stations.json')
ROOMS_FILE = os.path.join(DATA_DIR, 'rooms.json')
STATIC_DIR = os.path.join(os.path.dirname(__file__), 'static')

DEFAULT_REGION = os.environ.get('KEEGAN_DEFAULT_REGION', 'us-midwest')
REGISTRY_KEY = os.environ.get('KEEGAN_REGISTRY_KEY')
REGISTRY_VERSION = os.environ.get('REGISTRY_VERSION', '0.2.0')

DEFAULT_ALLOWED_ORIGINS = [
    'http://localhost:3000',
    'http://127.0.0.1:3000',
    'http://localhost:5173',
    'http://127.0.0.1:5173',
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
        self.send_header('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Api-Key')

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
