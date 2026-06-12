from http.server import HTTPServer, BaseHTTPRequestHandler

class SimpleHTTPRequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.end_headers()
        # Simulated comma-delimited response: QR_PAYLOAD,DATE_STRING
        self.wfile.write(b"W0721439019,Jun 10, 2026")

httpd = HTTPServer(('0.0.0.0', 8080), SimpleHTTPRequestHandler)
print("Listening on port 8080...")
httpd.serve_forever()
