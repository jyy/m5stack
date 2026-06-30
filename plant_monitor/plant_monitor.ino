#include <M5Unified.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Preferences.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <vector>

M5Canvas canvas(&M5.Display);
Preferences preferences;

String targetMac = ""; // Configure via Serial monitor on first boot
bool tempReceived = false;
bool moistReceived = false;
int currentMoisture = -1;
float currentTemp = -1000.0;

std::vector<String> foundSensors;

class SetupScanCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        // Filter specifically for Mi Flora devices (UUID 0xFE95) to prevent clutter
        bool isFlora = false;
        
        uint8_t* payload = advertisedDevice.getPayload();
        size_t payloadLength = advertisedDevice.getPayloadLength();
        size_t offset = 0;
        
        while (offset < payloadLength) {
            uint8_t length = payload[offset];
            if (length == 0) break;
            
            uint8_t type = payload[offset + 1];
            if (type == 0x16) { // Service Data
                uint16_t uuid = payload[offset + 2] | (payload[offset + 3] << 8);
                if (uuid == 0xFE95) {
                    isFlora = true;
                    break;
                }
            }
            offset += length + 1;
        }

        if (isFlora) {
            String mac = String(advertisedDevice.getAddress().toString().c_str());
            mac.toLowerCase();
            // Add if not already in vector
            bool exists = false;
            for (String s : foundSensors) {
                if (s == mac) { exists = true; break; }
            }
            if (!exists) {
                foundSensors.push_back(mac);
                Serial.println("Found MiFlora: " + mac);
            }
        }
    }
};

class MonitorScanCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        String addr = String(advertisedDevice.getAddress().toString().c_str());
        addr.toLowerCase();
        
        if (addr == targetMac) {
            uint8_t* payload = advertisedDevice.getPayload();
            size_t payloadLength = advertisedDevice.getPayloadLength();
            size_t offset = 0;
            
            while (offset < payloadLength) {
                uint8_t length = payload[offset];
                if (length == 0) break;
                
                uint8_t type = payload[offset + 1];
                if (type == 0x16) { // Service Data
                    uint16_t uuid = payload[offset + 2] | (payload[offset + 3] << 8);
                    if (uuid == 0xFE95) {
                        if (length < 5) break; // Bounds check
                        int dataOffset = offset + 4;
                        int frameOffset = dataOffset + 5; 
                        if (payload[dataOffset] & 0x10) frameOffset += 6; 
                        if (payload[dataOffset] & 0x20) frameOffset += 1; 
                        
                        // Ensure it has data (bit 6 of Frame Control is set)
                        if ((payload[dataOffset] & 0x40) == 0) break;
                        
                        if (offset + length >= frameOffset + 3) {
                            uint16_t eventID = payload[frameOffset] | (payload[frameOffset + 1] << 8);
                            uint8_t dataLength = payload[frameOffset + 2];
                            int valOffset = frameOffset + 3;

                            if (eventID == 0x1004 && dataLength == 2) {
                                currentTemp = (payload[valOffset] | (payload[valOffset + 1] << 8)) / 10.0f;
                                tempReceived = true;
                            } else if (eventID == 0x1008 && dataLength == 1) {
                                currentMoisture = payload[valOffset];
                                moistReceived = true;
                            } else if (eventID == 0x100D && dataLength == 3) {
                                currentTemp = (payload[valOffset] | (payload[valOffset + 1] << 8)) / 10.0f;
                                currentMoisture = payload[valOffset + 2];
                                tempReceived = true;
                                moistReceived = true;
                            }
                        }
                    }
                }
                offset += length + 1;
            }

            // Removed old dataReceived logic
        }
    }
};

void drawMenu(int selectedOption) {
    canvas.clear(TFT_WHITE);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextDatum(top_left);
    
    int rectHeight = 40;
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
    canvas.drawString("Select Sensor:", 10, 5);
    
    int startY = 30;
    
    for (int i = 0; i < foundSensors.size(); i++) {
        uint16_t fgColor = (selectedOption == i) ? TFT_WHITE : TFT_BLACK;
        uint16_t bgColor = (selectedOption == i) ? TFT_BLACK : TFT_WHITE;
        
        canvas.setTextColor(fgColor, bgColor);
        canvas.fillRect(0, startY + (i * rectHeight), 200, rectHeight, bgColor);
        
        int textX = 20;
        if (selectedOption == i) {
            int yCenter = startY + (i * rectHeight) + (rectHeight / 2);
            int xLeft = 5;
            canvas.fillTriangle(xLeft, yCenter - 6, xLeft, yCenter + 6, xLeft + 8, yCenter, fgColor);
        }
        
        canvas.drawString(foundSensors[i].c_str(), textX, startY + (i * rectHeight) + 12);
    }

    canvas.pushSprite(0, 0);
}

void runSetupMode() {
    M5.Display.setEpdMode(epd_mode_t::epd_fast); // Use fast mode for menu UI
    
    while (true) {
        foundSensors.clear();
        canvas.clear(TFT_WHITE);
        canvas.setTextColor(TFT_BLACK, TFT_WHITE);
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.drawCenterString("Scanning for", 100, 80);
        canvas.drawCenterString("Mi Flora sensors...", 100, 110);
        canvas.pushSprite(0, 0);

        BLEDevice::init("");
        BLEScan* pBLEScan = BLEDevice::getScan();
        pBLEScan->setAdvertisedDeviceCallbacks(new SetupScanCallbacks());
        pBLEScan->setActiveScan(true);
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(99);
        
        pBLEScan->start(5, false); // Scan for 5 seconds
        
        if (foundSensors.size() == 0) {
            canvas.clear(TFT_WHITE);
            canvas.drawCenterString("No sensors found!", 100, 80);
            canvas.drawCenterString("Press wheel to retry", 100, 110);
            canvas.pushSprite(0, 0);
            
            while (true) {
                M5.update();
                if (M5.BtnB.wasPressed() || M5.BtnEXT.wasPressed()) break;
                delay(50);
            }
            continue; // retry scan
        }
        
        // Enter menu
        int selectedOption = 0;
        drawMenu(selectedOption);
        
        while (true) {
            M5.update();
            bool menuChanged = false;
            
            if (M5.BtnA.wasPressed()) { // Up
                selectedOption--;
                if (selectedOption < 0) selectedOption = foundSensors.size() - 1;
                menuChanged = true;
            }
            if (M5.BtnC.wasPressed()) { // Down
                selectedOption++;
                if (selectedOption >= foundSensors.size()) selectedOption = 0;
                menuChanged = true;
            }
            
            if (M5.BtnB.wasPressed() || M5.BtnEXT.wasPressed()) {
                // Save and exit
                targetMac = foundSensors[selectedOption];
                preferences.begin("plant", false);
                preferences.putString("target_mac", targetMac);
                preferences.end();
                
                canvas.clear(TFT_WHITE);
                canvas.drawCenterString("Saved!", 100, 100);
                canvas.pushSprite(0, 0);
                delay(2000);
                
                // Reboot cleanly into normal monitoring mode
                esp_restart();
            }
            
            if (menuChanged) {
                drawMenu(selectedOption);
            }
            
            delay(50);
        }
    }
}

void drawGrimace(int cx, int cy) {
    canvas.drawCircle(cx, cy, 35, TFT_BLACK);
    // Eyes
    canvas.fillCircle(cx - 12, cy - 10, 4, TFT_BLACK);
    canvas.fillCircle(cx + 12, cy - 10, 4, TFT_BLACK);
    // Mouth
    canvas.drawRect(cx - 18, cy + 5, 36, 14, TFT_BLACK);
    canvas.drawLine(cx - 18, cy + 12, cx + 18, cy + 12, TFT_BLACK); 
    for (int i = cx - 14; i <= cx + 14; i += 4) {
        canvas.drawLine(i, cy + 5, i, cy + 19, TFT_BLACK); 
    }
}

void drawCrying(int cx, int cy) {
    canvas.drawCircle(cx, cy, 35, TFT_BLACK);
    // Left eye >
    canvas.drawLine(cx - 20, cy - 12, cx - 10, cy - 7, TFT_BLACK);
    canvas.drawLine(cx - 20, cy - 2, cx - 10, cy - 7, TFT_BLACK);
    // Right eye <
    canvas.drawLine(cx + 20, cy - 12, cx + 10, cy - 7, TFT_BLACK);
    canvas.drawLine(cx + 20, cy - 2, cx + 10, cy - 7, TFT_BLACK);
    // Dithered Tears
    for (int y = cy; y < cy + 18; y++) {
        for (int x = cx - 18; x < cx - 12; x++) {
            if ((x + y) % 2 == 0) canvas.drawPixel(x, y, TFT_BLACK);
        }
        for (int x = cx + 12; x < cx + 18; x++) {
            if ((x + y) % 2 == 0) canvas.drawPixel(x, y, TFT_BLACK);
        }
    }
    // Wailing mouth
    canvas.fillCircle(cx, cy + 14, 9, TFT_BLACK);
}

void drawBatteryIcon(int x, int y, int percentage) {
    // Battery body (30x14)
    canvas.drawRect(x, y, 30, 14, TFT_BLACK);
    // Battery terminal nub on the right
    canvas.fillRect(x + 30, y + 4, 3, 6, TFT_BLACK);
    
    // Fill the battery proportionally
    int fillWidth = (26 * percentage) / 100;
    if (fillWidth > 0) {
        canvas.fillRect(x + 2, y + 2, fillWidth, 10, TFT_BLACK);
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    canvas.createSprite(M5.Display.width(), M5.Display.height());
    
    preferences.begin("plant", true);
    targetMac = preferences.getString("target_mac", "");
    preferences.end();

    // Check if we need to enter setup mode (no MAC saved, or user is holding the push wheel)
    M5.update();
    if (targetMac == "" || M5.BtnB.isPressed()) {
        runSetupMode();
    }

    // Normal Monitoring Mode
    M5.Display.setEpdMode(epd_mode_t::epd_fast); // Fast mode to prevent full screen flashes
    
    BLEDevice::init("");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MonitorScanCallbacks());
    pBLEScan->setActiveScan(true); // Active scan required to get scan response payload
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    int batLevel = M5.Power.getBatteryLevel();
    char batStr[16];
    snprintf(batStr, sizeof(batStr), "%d%%", batLevel);
    
    int attempt = 1;
    while (!moistReceived && attempt <= 15) { // Try for up to 30 seconds
        canvas.clear(TFT_WHITE);
        canvas.setTextColor(TFT_BLACK, TFT_WHITE);
        
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.drawRightString(batStr, 160, 10);
        drawBatteryIcon(163, 11, batLevel);
        
        drawGrimace(100, 75); 
        
        canvas.setFont(&fonts::FreeSansBold12pt7b);
        char msg[64];
        snprintf(msg, sizeof(msg), "SCANNING (%d)", attempt);
        canvas.drawCenterString(msg, 100, 135);
        
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.drawCenterString("Moisture: -%", 100, 165);
        
        canvas.pushSprite(0, 0);
        M5.Display.waitDisplay(); // Crucial: wait for E-ink to completely finish before turning on BLE

        Serial.printf("Starting blocking BLE scan %d...\n", attempt);
        pBLEScan->start(2, false); // Block and scan for 2s
        pBLEScan->clearResults();
        attempt++;
    }
    canvas.clear(TFT_WHITE);
    
    // Draw battery level and iPhone-style icon in top right corner
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.drawRightString(batStr, 160, 10);
    drawBatteryIcon(163, 11, batLevel);
    
    if (tempReceived || moistReceived) {
        if (currentMoisture < 15) {
            drawCrying(100, 75); 
            canvas.setFont(&fonts::FreeSansBold12pt7b);
            canvas.drawCenterString("WATER ME!", 100, 135);
        } else {
            drawGrimace(100, 75); 
            canvas.setFont(&fonts::FreeSansBold12pt7b);
            canvas.drawCenterString("ALL GOOD", 100, 135);
        }
        
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        char moistStr[64];
        snprintf(moistStr, sizeof(moistStr), "Moisture: %d%%", currentMoisture);
        canvas.drawCenterString(moistStr, 100, 165);
    } else {
        canvas.setFont(&fonts::FreeSansBold18pt7b);
        canvas.drawCenterString("NO DATA", 100, 70);
        
        canvas.setFont(&fonts::FreeSansBold9pt7b);
        canvas.drawCenterString("Sensor not found", 100, 130);
    }

    // Keep using epd_fast to avoid the annoying multi-flash inversion
    M5.Display.setEpdMode(epd_mode_t::epd_fast); 
    canvas.pushSprite(0, 0);
    M5.Display.waitDisplay(); // Wait for E-ink physical refresh
    canvas.setFont(nullptr);
    
    Serial.println("Going to sleep...");
    delay(1000); 
    
    // On CoreInk, standard deep sleep drops the HOLD pin which cuts battery power entirely.
    // We MUST use M5.Power.timerSleep so the onboard RTC chip wakes it back up!
    M5.Power.timerSleep(86400); // 24 hours
}

void loop() {
}
