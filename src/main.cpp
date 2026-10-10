#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#define GFX_BL 2 // Rétroéclairage

Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    GFX_NOT_DEFINED /* CS */, GFX_NOT_DEFINED /* SCK */, GFX_NOT_DEFINED /* SDA */,
    41 /* DE */, 40 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    14 /* R0 */, 21 /* R1 */, 47 /* R2 */, 48 /* R3 */, 45 /* R4 */,
    9  /* G0 */, 46 /* G1 */, 3  /* G2 */, 8  /* G3 */, 16 /* G4 */, 1  /* G5 */,
    15 /* B0 */, 7  /* B1 */, 6  /* B2 */, 5  /* B3 */, 4  /* B4 */,
    0 /* hsync_polarity */, 210 /* hsync_front_porch */, 30 /* hsync_pulse_width */, 16 /* hsync_back_porch */,
    0 /* vsync_polarity */, 22 /* vsync_front_porch */, 13 /* vsync_pulse_width */, 10 /* vsync_back_porch */
);

// Utilisation de l'ancienne nomenclature compatible avec la v1.4.6
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    800 /* width */, 480 /* height */, bus, 0 /* rotation */, true /* auto_flush */
);

void setup() {
    Serial.printf("PSRAM: %d bytes, Free heap: %d\n", ESP.getPsramSize(), ESP.getFreeHeap());
    Serial.begin(115200);

    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH); // Allume le rétroéclairage

    if (!gfx->begin()) {
        Serial.println("Erreur init affichage !");
        while (1);
    }

    gfx->fillScreen(RED);
    delay(1000);
    gfx->fillScreen(GREEN);
    delay(1000);
    gfx->fillScreen(BLUE);
    delay(1000);
    gfx->fillScreen(BLACK);     

    gfx->setTextColor(WHITE);
    gfx->setTextSize(3);
    gfx->setCursor(100, 200);
    gfx->println("ESP32-8048S070 OK !");
}

void loop() {
}