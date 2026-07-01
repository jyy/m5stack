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

// Define a structure to hold each plant's data
struct PlantProbe {
    String name;
    String mac;
    int moisture = -1;
    bool found = false;
};

#if __has_include("secrets.h")
#include "secrets.h"
#else
#define SECRET_PROBES { \
    {"Plant 1", "aa:bb:cc:dd:ee:01"}, \
    {"Plant 2", "aa:bb:cc:dd:ee:02"}, \
    {"Plant 3", "aa:bb:cc:dd:ee:03"}, \
    {"Probe 4", ""}, \
    {"Probe 5", ""}, \
    {"Probe 6", ""} \
}
#endif

std::vector<PlantProbe> probes = SECRET_PROBES;

// Empty callback to clear results from memory

volatile int totalFound = 0;

class MonitorScanCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        String addr = advertisedDevice.getAddress().toString();
        
        bool parsed = false;
        int foundMoisture = -1;

        // Check if the discovered device matches any of our configured probes
        for (int i = 0; i < probes.size(); i++) {
            if (!probes[i].found && probes[i].mac != "") {
                // Zero-allocation case-insensitive match to prevent stack overflows in the background BLE task
                if (strcasecmp(addr.c_str(), probes[i].mac.c_str()) == 0) {
                
                    // Only parse the payload once per advertisement packet
                    if (!parsed) {
                        uint8_t* payload = advertisedDevice.getPayload();
                        size_t payloadLength = advertisedDevice.getPayloadLength();
                        size_t offset = 0;
                        
                        while (offset < payloadLength) {
                            uint8_t length = payload[offset];
                            if (length == 0) break;
                            
                            uint8_t type = payload[offset + 1];
                            if (type == 0x16) { // Service Data
                                uint16_t uuid = payload[offset + 2] | (payload[offset + 3] << 8);
                                if (uuid == 0xFE95) { // Mi Flora Service
                                    if (length < 5) break; 
                                    int dataOffset = offset + 4;
                                    int frameOffset = dataOffset + 5; 
                                    if (payload[dataOffset] & 0x10) frameOffset += 6; 
                                    if (payload[dataOffset] & 0x20) frameOffset += 1; 
                                    
                                    // Ensure it has data (bit 6 of Frame Control is set)
                                    if ((payload[dataOffset] & 0x40) != 0 && offset + length >= frameOffset + 3) {
                                        uint16_t eventID = payload[frameOffset] | (payload[frameOffset + 1] << 8);
                                        uint8_t dataLength = payload[frameOffset + 2];
                                        int valOffset = frameOffset + 3;

                                        if (eventID == 0x1008 && dataLength == 1) { // Moisture only event
                                            foundMoisture = payload[valOffset];
                                        } else if (eventID == 0x100D && dataLength == 3) { // Temp + Moisture event
                                            foundMoisture = payload[valOffset + 2];
                                        }
                                    }
                                    break; // Found Mi Flora service data, stop parsing other service data
                                }
                            }
                            offset += length + 1;
                        }
                        parsed = true;
                    }

                    // If we found moisture data, apply it to this probe
                    if (foundMoisture != -1) {
                        probes[i].moisture = foundMoisture;
                        probes[i].found = true;
                        totalFound++;
                    }
                } // End of strcasecmp if
            } // End of probes[i].found check
        } // End of for loop
    } // End of onResult
};


// Micro versions of the emojis (10px radius to fit 6 rows)
void drawMiniSmiley(int cx, int cy) {
    // Thicker head
    canvas.drawCircle(cx, cy, 10, TFT_BLACK);
    canvas.drawCircle(cx, cy, 9, TFT_BLACK);
    // Thicker eyes
    canvas.fillCircle(cx - 4, cy - 3, 2, TFT_BLACK);
    canvas.fillCircle(cx + 4, cy - 3, 2, TFT_BLACK);
    // Thicker smile (double lined)
    canvas.drawLine(cx - 5, cy + 2, cx - 3, cy + 4, TFT_BLACK);
    canvas.drawLine(cx - 5, cy + 3, cx - 3, cy + 5, TFT_BLACK);
    canvas.drawLine(cx - 3, cy + 4, cx + 3, cy + 4, TFT_BLACK);
    canvas.drawLine(cx - 3, cy + 5, cx + 3, cy + 5, TFT_BLACK);
    canvas.drawLine(cx + 3, cy + 4, cx + 5, cy + 2, TFT_BLACK);
    canvas.drawLine(cx + 3, cy + 5, cx + 5, cy + 3, TFT_BLACK);
}

void drawMiniWaterDrop(int cx, int cy) {
    canvas.fillCircle(cx, cy + 2, 6, TFT_BLACK);
    canvas.fillTriangle(cx - 6, cy + 2, cx + 6, cy + 2, cx, cy - 8, TFT_BLACK);
    // Little highlight
    canvas.drawPixel(cx - 2, cy + 3, TFT_WHITE);
    canvas.drawPixel(cx - 2, cy + 2, TFT_WHITE);
    canvas.drawPixel(cx - 1, cy + 1, TFT_WHITE);
}

void drawMiniQuestion(int cx, int cy) {
    canvas.drawCircle(cx, cy, 10, TFT_BLACK);
    canvas.setTextDatum(middle_center);
    canvas.drawString("?", cx, cy);
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    canvas.createSprite(M5.Display.width(), M5.Display.height());
    M5.Display.setEpdMode(epd_mode_t::epd_fast); 
    
    BLEDevice::init("");
    BLEScan* pBLEScan = BLEDevice::getScan();
    // Setting wantDuplicates to true prevents BLEScan from permanently storing devices in its internal map, solving the memory leak!
    pBLEScan->setAdvertisedDeviceCallbacks(new MonitorScanCallbacks(), true);
    pBLEScan->setActiveScan(true); 
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);

    int batLevel = M5.Power.getBatteryLevel();
    // Calculate battery bar width (max 200 pixels)
    int batWidth = (batLevel * 200) / 100;
    
    int targetFound = 0;
    for (int i = 0; i < probes.size(); i++) {
        if (probes[i].mac != "") targetFound++;
    }
    
    int attempt = 1;
    
    // Start the BLE scan asynchronously (runs in background for 90 seconds)
    // This prevents the massive RF calibration current spikes that happen every time start() is called
    // and eliminates the memory leak from repeatedly stopping and starting the scan.
    pBLEScan->clearResults(); 
    
    // Unified dynamic rendering loop
    while (true) {
        canvas.clear(TFT_WHITE);
        canvas.setTextColor(TFT_BLACK, TFT_WHITE);
        
        // Draw 3-pixel battery bar at the very top
        if (batWidth > 0) canvas.fillRect(0, 0, batWidth, 3, TFT_BLACK);
        
        // Dynamically calculate row height to evenly distribute the active probes
        int startY = 4;
        int activeProbes = (targetFound > 0) ? targetFound : 1;
        int rowHeight = (200 - startY) / activeProbes;
        
        int drawIndex = 0;
        for (int i = 0; i < probes.size(); i++) {
            if (probes[i].mac == "") continue; // Skip unconfigured probes entirely
            
            // Force reset the datum at the start of every row
            canvas.setTextDatum(middle_left);
            
            int yCenter = startY + (drawIndex * rowHeight) + (rowHeight / 2);
            drawIndex++;
            
            // Print Name (e.g. "Probe 1") using a smaller font, flush left to the screen (x=2)
            canvas.setFont(&fonts::FreeSansBold9pt7b);
            canvas.drawString(probes[i].name.c_str(), 2, yCenter);
            
            // Restore larger font for percentages
            canvas.setFont(&fonts::FreeSansBold12pt7b);
            
            if (probes[i].found) {
                char mStr[16];
                snprintf(mStr, sizeof(mStr), "%d%%", probes[i].moisture);
                canvas.setTextDatum(middle_right);
                canvas.drawString(mStr, 170, yCenter);
                canvas.setTextDatum(middle_left); // Reset datum
                
                if (probes[i].moisture < 15) {
                    drawMiniWaterDrop(188, yCenter);
                } else {
                    drawMiniSmiley(188, yCenter);
                }
            } else {
                // Not found yet (either scanning or failed)
                canvas.setTextDatum(middle_right);
                canvas.drawString("- %", 170, yCenter);
                canvas.setTextDatum(middle_left); // Reset datum
                
                if (attempt <= 45) {
                    canvas.setTextDatum(middle_left);
                    int dots = attempt % 3;
                    if (dots == 1) canvas.drawString(".", 176, yCenter);
                    else if (dots == 2) canvas.drawString("..", 176, yCenter);
                    else canvas.drawString("...", 176, yCenter);
                } else {
                    drawMiniQuestion(188, yCenter);
                    canvas.setTextDatum(middle_left); // Reset datum
                }
            }
        }
        
        // Push the image to the e-ink screen
        M5.Display.setEpdMode(epd_mode_t::epd_fast); 
        canvas.pushSprite(0, 0);
        M5.Display.waitDisplay();
        
        // Exit check: If we found everything or maxed out attempts
        if (totalFound >= targetFound || attempt > 45) {
            pBLEScan->clearResults();
            break;
        }
        
        // Wait 2 seconds before drawing the next frame, while BLE scans in the background
        delay(3000); // 3 second battery recovery
        pBLEScan->start(2, false);
        pBLEScan->clearResults(); 
        attempt++;
    }
    
    delay(1000); 
    // Go to sleep for 24 hours. The RTC will wake it up automatically.
    M5.Power.timerSleep(86400); 
}

void loop() {
    // Left empty because we use deep sleep
}
