#include <M5Unified.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <lgfx/utility/lgfx_qrcode.h>

// Configuration is loaded from config.h (which is ignored by git)
#include "config.h"

// UI State
int selectedOption = 0; // 0 = Purchase, 1 = Show Last, 2 = Turn Off
M5Canvas canvas(&M5.Display);
Preferences preferences;

void drawMenu() {
  canvas.clear(TFT_WHITE);
  
  // FreeSansBold9pt7b is bold enough to be very readable, 
  // but small enough that 16 characters won't overflow the 200px width.
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(top_left);
  
  // Option 0: Starts at top of screen (y=0)
  if (selectedOption == 0) {
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.fillRect(0, 0, 200, 45, TFT_BLACK);
  } else {
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  }
  // Center text vertically in the 45px rect
  canvas.drawString("Purchase parking", 10, 16);
  
  // Option 1: Stacked directly below
  if (selectedOption == 1) {
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.fillRect(0, 45, 200, 45, TFT_BLACK);
  } else {
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  }
  canvas.drawString("Show last parking", 10, 61);
  
  // Option 2: Stacked directly below
  if (selectedOption == 2) {
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.fillRect(0, 90, 200, 45, TFT_BLACK);
  } else {
    canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  }
  canvas.drawString("Turn off", 10, 106);

  // Push to screen using the fast EPD mode
  canvas.pushSprite(0, 0);
  
  // Reset font so the rest of the application uses defaults
  canvas.setFont(nullptr);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  canvas.createSprite(M5.Display.width(), M5.Display.height());
  M5.Display.setEpdMode(epd_mode_t::epd_fast);

  drawMenu();
}

void drawHighEccQRCode(M5Canvas& canvas, const char* string, int32_t x, int32_t y, int32_t w) {
  uint8_t version = 1;
  for (; version <= 40; ++version) {
    QRCode qrcode;
    uint16_t bufSize = lgfx_qrcode_getBufferSize(version);
    uint8_t* qrcodeData = (uint8_t*)malloc(bufSize);
    
    // ECC_HIGH = 3. This provides up to 30% error correction capability!
    if (0 != lgfx_qrcode_initText(&qrcode, qrcodeData, version, 3, string)) {
      free(qrcodeData);
      continue; // String too large for this version, try the next size up
    }
    
    int_fast16_t thickness = w / qrcode.size;
    int_fast16_t lineLength = qrcode.size * thickness;
    int_fast16_t xpos = x + ((w - lineLength) >> 1);
    int_fast16_t ypos = y + ((w - lineLength) >> 1);
    
    canvas.fillRect(x, y, w, w, TFT_WHITE);
    
    int_fast16_t iy = 0;
    do {
      int_fast16_t ix = 0;
      do {
        if (lgfx_qrcode_getModule(&qrcode, ix, iy)) {
          canvas.fillRect(xpos + ix * thickness, ypos + iy * thickness, thickness, thickness, TFT_BLACK);
        }
      } while (++ix < qrcode.size);
    } while (++iy < qrcode.size);
    
    free(qrcodeData);
    break;
  }
}

void renderFinalScreen(String dateStr, String payload) {
  // Set back to Quality mode for the final image to ensure no ghosting remains
  M5.Display.setEpdMode(epd_mode_t::epd_quality);
  
  canvas.clear(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  
  // Increase QR size to 150px
  int qrSize = 150;
  int x = (200 - qrSize) / 2;
  int qrY = 25; 
  
  // Set bold font
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  
  canvas.drawCenterString(dateStr.c_str(), 100, 5);
  
  // Use our custom High ECC drawing function
  drawHighEccQRCode(canvas, payload.c_str(), x, qrY, qrSize);
  
  canvas.drawCenterString(payload.c_str(), 100, qrY + qrSize + 5);
  
  // Reset font
  canvas.setFont(nullptr);
  
  // Push everything to the screen in a single refresh cycle!
  canvas.pushSprite(0, 0);
  
  delay(3000);
  M5.Power.powerOff();
}

void handleShowLastParking() {
  preferences.begin("parking", true); // true = Read Only
  String payload = preferences.getString("last_qr", "");
  String dateStr = preferences.getString("last_date", "");
  preferences.end();
  
  if (payload == "") {
    // If memory is blank, warn the user and return to menu
    M5.Display.clear(TFT_WHITE);
    M5.Display.drawCenterString("No saved parking!", 100, 100, 2);
    delay(3000);
    
    // Redraw menu
    drawMenu();
    return;
  }
  
  // Instantly render the cached strings!
  renderFinalScreen(dateStr, payload);
}

void handlePurchaseParking() {
  M5.Display.clear(TFT_WHITE);
  M5.Display.drawCenterString("Connecting to WiFi...", 100, 100, 2);
  
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    M5.Display.clear(TFT_WHITE);
    M5.Display.drawCenterString("WiFi Failed!", 100, 100, 4);
    delay(3000);
    drawMenu();
    return;
  }

  M5.Display.clear(TFT_WHITE);
  M5.Display.drawCenterString("Fetching data...", 100, 100, 2);
  
  HTTPClient http;
  http.begin(endpointUrl);
  int httpCode = http.GET();
  
  String payload = "";
  String dateStr = "Unknown Date";
  
  if (httpCode > 0) {
    String response = http.getString();
    
    // Split the comma delimited response: "QR_CODE,DATE"
    int commaIdx = response.indexOf(',');
    if (commaIdx != -1) {
      payload = response.substring(0, commaIdx);
      dateStr = response.substring(commaIdx + 1);
      
      // Trim any hidden whitespace or newlines
      payload.trim();
      dateStr.trim();
    } else {
      payload = response; // Fallback if no comma found
    }
  } else {
    M5.Display.clear(TFT_WHITE);
    M5.Display.drawCenterString("HTTP Error", 100, 100, 4);
    http.end();
    WiFi.disconnect();
    delay(3000);
    drawMenu();
    return;
  }
  http.end();
  WiFi.disconnect();
  
  // Cache the strings in NVS Memory for offline recall!
  preferences.begin("parking", false); // false = Read/Write
  preferences.putString("last_qr", payload);
  preferences.putString("last_date", dateStr);
  preferences.end();
  
  renderFinalScreen(dateStr, payload);
}

void loop() {
  M5.update();
  
  bool menuChanged = false;

  // Up
  if (M5.BtnA.wasPressed()) {
    selectedOption--;
    if (selectedOption < 0) selectedOption = 2; // Cycle back to bottom
    menuChanged = true;
  }
  
  // Down
  if (M5.BtnC.wasPressed()) {
    selectedOption++;
    if (selectedOption > 2) selectedOption = 0; // Cycle back to top
    menuChanged = true;
  }
  
  // Select
  if (M5.BtnB.wasPressed() || M5.BtnEXT.wasPressed()) {
    if (selectedOption == 0) {
      handlePurchaseParking();
    } else if (selectedOption == 1) {
      handleShowLastParking();
    } else {
      M5.Display.setEpdMode(epd_mode_t::epd_quality);
      M5.Display.clear(TFT_WHITE);
      M5.Display.drawCenterString("Powering Off...", 100, 100, 2);
      delay(2000);
      M5.Power.powerOff();
    }
  }

  if (menuChanged) {
    drawMenu();
  }
}
