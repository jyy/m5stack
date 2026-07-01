# M5Stack CoreInk Multi-Plant Monitor

This project turns an M5Stack CoreInk (ESP32) into an ultra-low-power, E-ink multi-plant moisture monitor. It connects via Bluetooth Low Energy (BLE) to up to 6 Xiaomi Mi Flora plant sensors, reads their soil moisture, and displays a dynamic dashboard before going into deep sleep for 24 hours.

## Hardware
* **M5Stack CoreInk**: An ESP32-based development board with a 1.54" E-ink display and a 390mAh battery.
* **Xiaomi Mi Flora Sensors**: BLE plant sensors that broadcast temperature, moisture, light, and conductivity.

## Features
* **Dynamic Auto-Scaling UI**: The dashboard automatically calculates the screen height and row spacing based on the number of configured MAC addresses, ensuring the layout is always perfectly spaced whether you have 1 or 6 plants configured.
* **CLI-Style Progress Indicator**: While scanning for a specific probe, it renders a sleek `.` `..` `...` animated indicator to show active work without redrawing the whole screen.
* **Custom Primitive E-ink Emojis**: We built custom geometric emojis using `M5GFX` primitives to display the plant's mood.
    * 🙂 **Smiley Face (ALL GOOD)**: Displayed when moisture is >= 15%.
    * 💧 **Water Drop (WATER ME!)**: Displayed when moisture drops below 15%.
    * ❓ **Question Mark (UNKNOWN)**: Displayed if a sensor is unconfigured or offline.
* **Dynamic Battery Indicator**: Draws a battery bar spanning the top of the screen, filling proportionally based on the CoreInk's internal ADC battery reading.
* **Ultra-Low Power Deep Sleep**: Uses the CoreInk's onboard RTC (`M5.Power.timerSleep`) to completely shut down the ESP32 and E-ink display for 24 hours between readings, extending battery life to potentially over a year.
* **Brownout Prevention**: The ESP32 Bluetooth radio and physical E-ink refresh engine both draw significant current. To prevent the tiny battery from browning out, the system forces sequential operation with a 3-second battery recovery period (`delay(3000)`) between the E-ink refresh and the BLE scan.
* **Zero-Leak Memory Architecture**: The Arduino BLE library is instructed not to store duplicate scanned devices (`wantDuplicates = true`), completely avoiding internal `std::map` heap fragmentation and memory leaks over long scanning sessions.

## How it Works
1. The CoreInk wakes up from the RTC timer (or manual button press).
2. It dynamically allocates rows and draws the initial layout with the battery bar and all configured plant names.
3. It enters a loop:
    * Refreshes the E-ink screen with the current states.
    * Waits 3 seconds for the battery voltage to stabilize.
    * Runs a blocking BLE scan for 2 seconds.
    * Intercepts Xiaomi Service Data (`0xFE95`) and parses the moisture percentage (Event IDs `0x100D` or `0x1008`).
    * Updates the UI state of any newly found probe and ticks the scanning animation.
4. Once all probes are found (or a maximum of 45 attempts is reached), the device goes into deep sleep for 86,400 seconds (24 hours).

## Setup & Flashing
Please refer to the [top-level README](../README.md) in the parent `m5stack` directory for detailed instructions on how to configure your partition scheme and compile/upload this project to the CoreInk.
