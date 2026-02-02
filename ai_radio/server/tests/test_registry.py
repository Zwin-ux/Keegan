import json
import os
import sys
import threading
import time
import unittest
from http.server import ThreadingHTTPServer
from urllib import request

sys.path.append(os.path.dirname(os.path.dirname(__file__)))

# Enable telemetry before importing the server module
os.environ['KEEGAN_TELEMETRY'] = '1'
os.environ['ALLOWED_ORIGINS'] = '*'
os.environ['KEEGAN_ANON_SESSION_MS'] = '200'
os.environ['KEEGAN_ANON_COOLDOWN_MS'] = '200'
os.environ['KEEGAN_INGEST_SECRET'] = 'test-secret'

import registry_server as registry


class RegistryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(('127.0.0.1', 0), registry.RegistryHandler)
        cls.port = cls.server.server_address[1]
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        time.sleep(0.1)

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.thread.join(timeout=1)

    def url(self, path):
        return f'http://127.0.0.1:{self.port}{path}'

    def request_json(self, path, method='GET', payload=None, headers=None):
        data = None
        if payload is not None:
            data = json.dumps(payload).encode('utf-8')
        req = request.Request(self.url(path), data=data, method=method)
        req.add_header('Content-Type', 'application/json')
        if headers:
            for key, value in headers.items():
                req.add_header(key, value)
        try:
            with request.urlopen(req, timeout=3) as resp:
                body = resp.read().decode('utf-8')
                return resp.status, json.loads(body) if body else {}
        except Exception as exc:
            if hasattr(exc, 'code'):
                body = exc.read().decode('utf-8') if hasattr(exc, 'read') else ''
                return exc.code, json.loads(body) if body else {}
            raise

    def test_health(self):
        status, payload = self.request_json('/health')
        self.assertEqual(status, 200)
        self.assertTrue(payload.get('ok'))

    def test_seed(self):
        status, payload = self.request_json('/api/seed')
        self.assertEqual(status, 200)
        self.assertTrue(payload.get('seed'))

    def test_station_upsert_and_list(self):
        status, payload = self.request_json('/api/stations', method='POST', payload={
            'name': 'Test Station',
            'region': 'us-midwest',
            'frequency': 98.7,
        })
        self.assertEqual(status, 200)
        station_id = payload.get('id')
        self.assertTrue(station_id)

        status, payload = self.request_json('/api/stations?region=us-midwest')
        self.assertEqual(status, 200)
        self.assertTrue(len(payload.get('stations', [])) >= 1)

    def test_rooms_presence(self):
        room_id = 'us-midwest|code|focus_room|2026-02-01'
        status, payload = self.request_json(f'/api/rooms/{room_id}/presence', method='POST', payload={
            'listenerId': 'listener_test',
            'action': 'join',
            'region': 'us-midwest',
            'appKey': 'code',
            'toneId': 'focus_room',
            'frequency': 98.7,
        })
        self.assertEqual(status, 200)
        self.assertEqual(payload.get('roomId'), room_id)

        status, payload = self.request_json('/api/rooms?region=us-midwest')
        self.assertEqual(status, 200)
        rooms = payload.get('rooms', [])
        self.assertTrue(any(r.get('roomId') == room_id for r in rooms))

    def test_telemetry_endpoint(self):
        status, payload = self.request_json('/api/telemetry', method='POST', payload={
            'event': 'test_event',
            'ts': int(time.time() * 1000),
            'source': 'test',
        })
        self.assertEqual(status, 200)
        self.assertTrue(payload.get('ok'))

    def test_web_anon_begin_and_cooldown(self):
        status, payload = self.request_json(
            '/api/stations/web/begin',
            method='POST',
            payload={'mode': 'anon', 'station': {'region': 'us-midwest'}},
            headers={'X-Client-Id': 'client_a'},
        )
        self.assertEqual(status, 200)
        self.assertEqual(payload.get('stationId'), 'anon')
        token = payload.get('token')
        self.assertTrue(token)
        session_id = payload.get('sessionId')

        status, payload = self.request_json('/api/stations/anon/status')
        self.assertEqual(status, 200)
        self.assertTrue(payload.get('broadcasting'))
        self.assertEqual(payload.get('sessionId'), session_id)

        status, payload = self.request_json(
            '/api/stations/web/begin',
            method='POST',
            payload={'mode': 'anon'},
            headers={'X-Client-Id': 'client_b'},
        )
        self.assertEqual(status, 409)

        status, payload = self.request_json(
            '/api/stations/web/stop',
            method='POST',
            payload={'token': token},
        )
        self.assertEqual(status, 200)

        status, payload = self.request_json(
            '/api/stations/web/begin',
            method='POST',
            payload={'mode': 'anon'},
            headers={'X-Client-Id': 'client_a'},
        )
        self.assertEqual(status, 429)

        time.sleep(0.25)
        status, payload = self.request_json(
            '/api/stations/web/begin',
            method='POST',
            payload={'mode': 'anon'},
            headers={'X-Client-Id': 'client_a'},
        )
        self.assertEqual(status, 200)


if __name__ == '__main__':
    unittest.main()
