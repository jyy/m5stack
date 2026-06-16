#include <M5Unified.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <lgfx/utility/lgfx_qrcode.h>

// Configuration is loaded from config.h (which is ignored by git)
#include "config.h"

M5Canvas canvas(&M5.Display);
Preferences preferences;

// Forward declarations for menu actions
void handlePurchase(const char* endpointUrl);
void handleShowLast(const char* dummy);
void handleTurnOff(const char* dummy);
void handleBack(const char* dummy);

// Dynamic Menu Structure
struct MenuItem {
  const char* title;
  struct MenuItem* children;
  int numChildren;
  void (*action)(const char* arg);
  const char* actionParam;
};

// Sub-menus
MenuItem purchaseMenu[] = {
  {"Today", nullptr, 0, handlePurchase, endpointToday},
  {"Tomorrow", nullptr, 0, handlePurchase, endpointTomorrow},
  {"Back", nullptr, 0, handleBack, nullptr}
};

// Main menu
MenuItem mainMenu[] = {
  {"Purchase parking", purchaseMenu, 3, nullptr, nullptr},
  {"Show last parking", nullptr, 0, handleShowLast, nullptr},
  {"Turn off", nullptr, 0, handleTurnOff, nullptr}
};

// Navigation State
MenuItem* currentMenu = mainMenu;
int currentMenuSize = 3;
int selectedOption = 0;

void drawMenu() {
  canvas.clear(TFT_WHITE);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(top_left);
  
  // Distribute items evenly. We'll use 45px per item
  int rectHeight = 45;
  
  for (int i = 0; i < currentMenuSize; i++) {
    uint16_t fgColor = (selectedOption == i) ? TFT_WHITE : TFT_BLACK;
    uint16_t bgColor = (selectedOption == i) ? TFT_BLACK : TFT_WHITE;
    
    canvas.setTextColor(fgColor, bgColor);
    canvas.fillRect(0, i * rectHeight, 200, rectHeight, bgColor);
    
    int textX = 10;

    // Draw a "Back" glyph (left-pointing triangle)
    if (currentMenu[i].action == handleBack) {
      int yCenter = (i * rectHeight) + (rectHeight / 2);
      int xLeft = 10;
      canvas.fillTriangle(xLeft, yCenter, xLeft + 6, yCenter - 6, xLeft + 6, yCenter + 6, fgColor);
      textX = 24; // Shift text over to make room for the arrow
    }
    
    // Center text vertically in the 45px rect
    canvas.drawString(currentMenu[i].title, textX, (i * rectHeight) + 16);
    
    // Draw a submenu glyph (right-pointing triangle) if this item expands
    if (currentMenu[i].children != nullptr) {
      int yCenter = (i * rectHeight) + (rectHeight / 2);
      int xRight = 180;
      canvas.fillTriangle(xRight, yCenter - 6, xRight, yCenter + 6, xRight + 6, yCenter, fgColor);
    }
  }

  // Push to screen using the fast EPD mode
  canvas.pushSprite(0, 0);
  canvas.setFont(nullptr);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  canvas.createSprite(M5.Display.width(), M5.Display.height());
  M5.Display.setEpdMode(epd_mode_t::epd_fast);

  drawMenu();
}

void loop() {
  M5.update();
  
  bool menuChanged = false;

  // Scroll Up
  if (M5.BtnA.wasPressed()) {
    selectedOption--;
    if (selectedOption < 0) selectedOption = currentMenuSize - 1; 
    menuChanged = true;
  }
  
  // Scroll Down
  if (M5.BtnC.wasPressed()) {
    selectedOption++;
    if (selectedOption >= currentMenuSize) selectedOption = 0; 
    menuChanged = true;
  }
  
  // Select Action
  if (M5.BtnB.wasPressed() || M5.BtnEXT.wasPressed()) {
    MenuItem& item = currentMenu[selectedOption];
    
    if (item.children != nullptr) {
      // Navigate into submenu
      currentMenu = item.children;
      currentMenuSize = item.numChildren;
      selectedOption = 0;
      menuChanged = true;
    } else if (item.action != nullptr) {
      // Execute the leaf node's action, passing its specific parameter
      item.action(item.actionParam);
    }
  }

  if (menuChanged) {
    drawMenu();
  }
}

// ---- Menu Actions below ----

void handleBack(const char* dummy) {
  // Simple state reset to main menu
  currentMenu = mainMenu;
  currentMenuSize = 3;
  selectedOption = 0;
  drawMenu();
}

void handleTurnOff(const char* dummy) {
  M5.Display.setEpdMode(epd_mode_t::epd_quality);
  M5.Display.clear(TFT_WHITE);
  M5.Display.drawCenterString("Powering Off...", 100, 100, 2);
  delay(2000);
  M5.Power.powerOff();
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

void handleShowLast(const char* dummy) {
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

void handlePurchase(const char* activeEndpointUrl) {
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
  
  // HTTPS Support
  WiFiClientSecure secureClient;
  secureClient.setInsecure(); // Disable strict certificate validation
  HTTPClient http;
  http.begin(secureClient, activeEndpointUrl);
  
  // Set timeout to 60 seconds (60000 ms) since backend generation can take 30-45s
  http.setTimeout(60000);
  
  // Apply any custom headers defined in config.h (e.g., Cloudflare Access tokens)
  for (int i = 0; i < numHttpHeaders; i++) {
    http.addHeader(httpHeaders[i].key, httpHeaders[i].value);
  }
  
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
