import json
import random
import time
import os
from http.server import BaseHTTPRequestHandler, HTTPServer

DYNAMIC_STORIES = {
    "focus": [
        "The algorithms are aligning perfectly tonight.",
        "Your focus is reshaping the digital landscape.",
        "Silence is the loudest frequency in the network.",
        "Compiling thoughts into pure crystal data."
    ],
    "rain": [
        "Each drop carries a memory of the ocean.",
        "The cave walls whisper ancient binary code.",
        "Storm systems approaching the outer perimeter.",
        "Humidity levels rising. Systems nominal."
    ],
    "arcade": [
        "High score detected in the neural lace.",
        "Neon lights flickering in sync with your heartbeat.",
        "Insert coin to continue the simulation.",
        "Pixels bleeding into reality."
    ],
    "sleep": [
        "Leaving orbit. Engaging hyper-sleep.",
        "The stars look different from this angle.",
        "Life support systems: calibrated for dreams.",
        "Drifting through the void of silence."
    ]
}

ALLOWED_ORIGINS = os.environ.get("ALLOWED_ORIGINS", "*")


class LLMHandler(BaseHTTPRequestHandler):
    def _send_cors_headers(self):
        self.send_header('Access-Control-Allow-Origin', ALLOWED_ORIGINS)
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')

    def do_OPTIONS(self):
        self.send_response(204)
        self._send_cors_headers()
        self.end_headers()

    def do_GET(self):
        if self.path in ('/api/state', '/state'):
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self._send_cors_headers()
            self.end_headers()
            response = {
                "mood": "focus",
                "energy": 0.5,
            }
            self.wfile.write(json.dumps(response).encode('utf-8'))
            return

        if self.path == '/health':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self._send_cors_headers()
            self.end_headers()
            self.wfile.write(json.dumps({"status": "ok"}).encode('utf-8'))
            return

        self.send_response(404)
        self.end_headers()

    def do_POST(self):
        if self.path in ('/generate', '/api/generate'):
            content_length = int(self.headers.get('Content-Length', 0))
            post_data = self.rfile.read(content_length)

            try:
                data = json.loads(post_data.decode('utf-8'))
                mood = data.get('mood', 'focus')

                stories = DYNAMIC_STORIES.get(mood, DYNAMIC_STORIES["focus"])
                text = random.choice(stories)

                timestamp = time.strftime("%H:%M:%S")

                response = {
                    "id": f"story_{int(time.time())}",
                    "text": f"{text} [{timestamp}]",
                }

                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                self._send_cors_headers()
                self.end_headers()
                self.wfile.write(json.dumps(response).encode('utf-8'))

            except Exception as e:
                self.send_response(500)
                self.send_header('Content-type', 'application/json')
                self._send_cors_headers()
                self.end_headers()
                self.wfile.write(json.dumps({"error": str(e)}).encode('utf-8'))
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        # Structured single-line log
        print(f"[{self.log_date_time_string()}] {format % args}")


def run(port=8080):
    httpd = HTTPServer(('', port), LLMHandler)
    print(f"Keegan LLM server listening on :{port}")
    httpd.serve_forever()


if __name__ == '__main__':
    port = int(os.environ.get("PORT", 8080))
    run(port=port)
