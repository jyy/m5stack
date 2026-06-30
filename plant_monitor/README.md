# M5Stack CoreInk Plant Monitor

This project turns an M5Stack CoreInk (ESP32) into an ultra-low-power, E-ink plant moisture monitor. It connects via Bluetooth Low Energy (BLE) to a Xiaomi Mi Flora plant sensor, reads the soil moisture, and displays a custom UI before going into deep sleep for 24 hours.

## Hardware
* **M5Stack CoreInk**: An ESP32-based development board with a 1.54" E-ink display and a 390mAh battery.
* **Xiaomi Mi Flora Sensor**: A BLE plant sensor that broadcasts temperature, moisture, light, and conductivity.

## Features
* **Custom Primitive E-ink Emojis**: Because the CoreInk doesn't support modern color fonts, we built custom geometric emojis using `M5GFX` primitives (circles, lines, and rectangles) to display the plant's mood.
    * 😬 **Grimace Face (ALL GOOD)**: Displayed when moisture is >= 15%.
    * 😭 **Loudly Crying Face (WATER ME!)**: Displayed when moisture drops below 15%. Features custom pixel-dithered tears.
* **Dynamic Battery Indicator**: Draws a custom iPhone-style battery icon in the top right, filling proportionally based on the CoreInk's internal ADC battery reading.
* **Ultra-Low Power Deep Sleep**: Uses the CoreInk's onboard RTC (`M5.Power.timerSleep`) to completely shut down the ESP32 and E-ink display for 24 hours between readings, extending battery life to potentially over a year.
* **On-Demand Refresh**: Pressing the physical power button on the CoreInk instantly wakes it from deep sleep, performs a real-time scan, updates the screen, and resets the 24-hour timer.
* **Brownout Prevention**: The ESP32 Bluetooth radio and physical E-ink refresh engine both draw significant current. To prevent the 390mAh battery from browning out, the system uses a sequential blocking scan. The E-ink screen fully settles *before* the BLE radio turns on, preventing power overlap.

## How it Works
1. The CoreInk wakes up from the RTC timer (or manual button press).
2. It draws the initial layout (battery icon, Grimace placeholder, `SCANNING (1)`, and `Moisture: -%`) and waits for the screen to settle.
3. It runs a blocking BLE scan for 2 seconds looking for your configured Mi Flora MAC address.
4. It intercepts the Xiaomi Service Data (`0xFE95`) and parses the moisture percentage (Event ID `0x100D`).
5. It ignores other data (like temperature) to keep the cycle fast and efficient.
6. It updates the layout to match the true moisture value (updating to Crying if < 15%) and re-draws the screen.
7. It goes back to deep sleep for 86,400 seconds (24 hours).

## Setup & Flashing
Please refer to the [top-level README](../README.md) in the parent `m5stack` directory for detailed instructions on how to configure your partition scheme and compile/upload this project to the CoreInk using either the Arduino IDE or `arduino-cli`.
