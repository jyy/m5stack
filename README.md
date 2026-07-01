# M5Stack Projects

This repository contains custom firmware and projects built for M5Stack devices.

## Projects

*   **[`plant_monitor`](./plant_monitor/)**: An ultra-low-power Bluetooth LE soil moisture monitor for the M5Stack CoreInk that reads data from a Xiaomi Mi Flora sensor and displays it on the E-ink screen.
*   **[`qr_display`](./qr_display/)**: (e.g. Parking Purchase display) A project designed to display QR codes or parking information.

## Compiling & Uploading

These projects target the **M5Stack CoreInk** (ESP32-PICO-D4) and use the `M5Unified` and `M5GFX` libraries.

### Using Arduino IDE

1.  Open the `.ino` sketch in Arduino IDE.
2.  Go to **Tools > Board** and select `M5Stack CoreInk` (under `M5Stack Arduino`).
3.  Go to **Tools > Partition Scheme** and select `Huge APP (3MB No OTA/1MB SPIFFS)`. *This is critical because the E-ink and BLE libraries are large.*
4.  Select your Serial Port (e.g., `/dev/cu.usbserial-...`).
5.  Click **Upload**.

### Using Arduino CLI

If you prefer the command line, you can compile and flash the projects using `arduino-cli`. 

**Compile:**
Navigate into the project directory and run:
```bash
arduino-cli compile --fqbn m5stack:esp32:m5stack_coreink --build-property "build.partitions=huge_app" --build-property "upload.maximum_size=3145728" .
```

**Upload:**
Find your CoreInk's serial port (e.g., `/dev/cu.usbserial-XXXXX`) and run:
```bash
arduino-cli upload -p <YOUR_SERIAL_PORT> --fqbn m5stack:esp32:m5stack_coreink .
```

*(Note: If you have installed `arduino-cli` internally via the Arduino IDE on macOS, the binary may be located at `/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli`)*

## Troubleshooting & Gotchas

*   **Fails to flash:** Ensure the device is turned on and not in deep sleep while attempting to flash. You can manually reset it by pressing the reset button on the side/back while the USB is plugged in.
*   **Brownouts:** The ESP32 Bluetooth radio and physical E-ink refresh engine both draw significant current. Projects using both BLE and E-ink should ensure sequential operation (using `M5.Display.waitDisplay()` and `delay(3000)`) to prevent the 390mAh battery from browning out. Do not run them concurrently.
*   **BLEScan Memory Leaks:** If your project continuously stops and restarts BLE scans in a loop, you **must** instruct the Arduino BLE library not to store duplicate devices. Pass `true` as the second argument: `pBLEScan->setAdvertisedDeviceCallbacks(..., true);`. Otherwise, its internal `std::map` will quickly fragment the heap and crash the device.
*   **M5GFX Text Clipping:** Adafruit GFX fonts (like `FreeSansBold`) have invisible bounding boxes that extend far below the visible text. If this bounding box clips the physical edge of the CoreInk's 200x200 screen, `M5GFX` will entirely discard the drawing command and the text will be mysteriously missing. Make sure to leave ample padding at the bottom of the screen.
*   **M5GFX Text Datum Pollution:** The `setTextDatum()` state is persistent in the canvas object. If a helper function (like drawing a centered UI element) modifies it to `middle_center`, you must manually reset it to `middle_left` (or whatever your default is) before drawing the next element. Otherwise, subsequent coordinates will be misaligned, often drawing text partially off the edge of the screen.
