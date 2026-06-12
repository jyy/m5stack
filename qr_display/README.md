# M5CoreInk Parking Pass Display

An elegant, offline-capable smart parking pass display built for the **M5Stack CoreInk** (ESP32 with an E-Ink display). 

This project allows you to instantly retrieve and display a parking pass QR code from a backend service over Wi-Fi. It's heavily optimized for the E-Ink display, featuring an anti-glare high-ECC QR code generator, fluid menus with zero ghosting, and offline caching so your pass survives reboots and dead batteries!

## Features

- **Fluid UI & Menu System**: Custom menu layout using `M5Canvas` and `epd_fast` mode to allow smooth scrolling without the jarring full-screen flash typical of E-Ink displays.
- **Offline Memory Recall**: The moment a parking pass is fetched via Wi-Fi, the payload and expiration date are silently cached to the ESP32's internal Non-Volatile Storage (NVS). You can recall your last parking pass instantly, completely offline!
- **High-Reliability QR Codes**: Bypasses the default graphics wrapper to inject `ECC_HIGH` (Level 3) error correction into the QR generator. This provides up to 30% error recovery, making the QR code effortlessly scannable even with extreme screen glare or poor lighting.
- **Ultra-Fast Network Fetch**: The device expects a simple comma-delimited string (`PAYLOAD,DATE`) from the backend, entirely eliminating the need for slow NTP time server syncs.
- **Graceful Error Handling**: Automatically catches Wi-Fi or HTTP failures, displays a 3-second warning, and seamlessly bounces the user back to the main menu without hard-locking or crashing.
- **Auto Power-Off**: Pushes the crisp final image to the screen using `epd_quality` mode, then physically cuts its own power to preserve the battery indefinitely.

## Requirements

- **Hardware**: M5Stack CoreInk (ESP32-PICO-D4)
- **Framework**: Arduino IDE with ESP32 board support (`m5stack_coreink`)
- **Libraries**: `M5Unified` (which automatically includes `M5GFX`)

## Setup

1. Open `qr_display.ino` in the Arduino IDE.
2. Create your configuration file:
   - Make a copy of `config.h.template` and rename it to `config.h`.
   - Open `config.h` and insert your network details:
     ```cpp
     const char* ssid = "your_wifi_ssid";
     const char* password = "your_wifi_password";
     const char* endpointUrl = "http://YOUR_SERVER_IP:8080/";
     ```
   *(Note: `config.h` is intentionally excluded by `.gitignore` so your credentials stay safe!)*
3. Compile and upload to your CoreInk!

## Backend Service Requirement

This device is designed to act as a pure, lightning-fast client. It expects your backend HTTP service to return a simple `text/plain` comma-delimited string formatted exactly as:
```text
QR_PAYLOAD,DATE_STRING
```

**Example Python Mock Server:**
You can test the device using this lightweight python script:

```python
from http.server import HTTPServer, BaseHTTPRequestHandler

class SimpleHTTPRequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.end_headers()
        # Simulated comma-delimited response
        self.wfile.write(b"W0721439019,Jun 10, 2026")

httpd = HTTPServer(('0.0.0.0', 8080), SimpleHTTPRequestHandler)
print("Listening on port 8080...")
httpd.serve_forever()
```
*(Alternatively, you can easily use `shell2http` to map a bash script directly to this endpoint!)*

## Usage
1. Use the scroll wheel (Up/Down) to navigate the menu.
2. Press the scroll wheel inwards (Select) to confirm.
3. Select **Purchase parking** to connect to Wi-Fi and fetch the newest pass.
4. Select **Show last parking** to instantly pull the cached pass from memory without turning on the Wi-Fi radio!
