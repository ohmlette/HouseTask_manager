#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <TAMC_GT911.h>
#include <math.h>

// =============================================================================
// 1. CONFIGURATION MATÉRIELLE (ESP32-8048S070 - Sunton 7" 800x480)
// =============================================================================
#define GFX_BL 2

#define TOUCH_SDA 19
#define TOUCH_SCL 20
#define TOUCH_INT 18
#define TOUCH_RST 38

#define SCREEN_W 800
#define SCREEN_H 480

// Bus RGB parallèle 16 bits
Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    41 /* DE */, 40 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    14 /* R0 */, 21 /* R1 */, 47 /* R2 */, 48 /* R3 */, 45 /* R4 */,
    9  /* G0 */, 46 /* G1 */, 3  /* G2 */, 8  /* G3 */, 16 /* G4 */, 1  /* G5 */,
    15 /* B0 */, 7  /* B1 */, 6  /* B2 */, 5  /* B3 */, 4  /* B4 */,
    0 /* hsync_polarity */, 180 /* hsync_front_porch */, 30 /* hsync_pulse_width */, 16 /* hsync_back_porch */,
    0 /* vsync_polarity */, 12 /* vsync_front_porch */, 13 /* vsync_pulse_width */, 10 /* vsync_back_porch */,
    1 /* pclk_active_neg */
);

// Affichage matériel DMA
Arduino_RGB_Display *rgbDisplay = new Arduino_RGB_Display(
    SCREEN_W, SCREEN_H, bus, 0 /* rotation */, true /* auto_flush */
);

// Double buffer en PSRAM (zéro scintillement !)
Arduino_Canvas *canvas = new Arduino_Canvas(SCREEN_W, SCREEN_H, rgbDisplay);
Arduino_GFX *gfx = canvas; // Pointeur de dessin actif

// Contrôleur tactile GT911
TAMC_GT911 tp = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, SCREEN_W, SCREEN_H);

// =============================================================================
// 2. PALETTE DE COULEURS HOLOGRAPHIQUES (RGB565)
// =============================================================================
#define COL_BG          RGB565(2, 6, 14)      // Noir abyssal bleuté
#define COL_GRID        RGB565(6, 22, 38)     // Grille tactique sombre

// Thème Cyan J.A.R.V.I.S.
#define CYAN_BRIGHT     RGB565(180, 255, 255) // Blanc-cyan cœur d'énergie
#define CYAN_NEON       RGB565(0, 235, 255)   // Cyan électrique principal
#define CYAN_MID        RGB565(0, 140, 210)   // Bleu-cyan intermédiaire
#define CYAN_DIM        RGB565(0, 65, 110)    // Cyan sombre structurel
#define GOLD_STARK      RGB565(255, 185, 0)   // Or Stark Industries

// Thème Rouge Combat / Overdrive
#define RED_BRIGHT      RGB565(255, 220, 180)
#define RED_NEON        RGB565(255, 45, 20)
#define RED_MID         RGB565(200, 20, 10)
#define RED_DIM         RGB565(90, 10, 5)
#define ORANGE_WARN     RGB565(255, 140, 0)

// =============================================================================
// 3. VARIABLES D'ÉTAT & ANIMATION
// =============================================================================
bool combatMode = false;
bool scanActive = false;
float scanLineY = 0;

float angleRing1 = 0.0f;
float angleRing2 = 0.0f;
float angleRing3 = 0.0f;
float angleRadar = 0.0f;
float pulsePhase = 0.0f;

// Dernière position tactile pour le réticule de visée
int16_t targetX = SCREEN_W / 2;
int16_t targetY = SCREEN_H / 2;
bool hasTouchTarget = false;
uint32_t lastTouchToggleMs = 0;
uint32_t frameCount = 0;
float currentFps = 30.0f;
uint32_t lastFpsMs = 0;

// Journal système J.A.R.V.I.S.
const char *jarvisLogs[] = {
    "> STARK HUD v85.4 ONLINE",
    "> PSRAM DOUBLE-BUFFER: OK",
    "> ARC REACTOR: 3.2 GJ/s",
    "> NANOTECH MATRIX: READY",
    "> SAT-LINK: ORBITAL 04",
    "> FLIGHT SYSTEMS: NOMINAL",
    "> DEFENSIVE GRID: ARMED",
    "> VOICE SYNTH: ACTIVE"
};

// =============================================================================
// 4. FONCTIONS GRAPHIQUES DU HUD J.A.R.V.I.S.
// =============================================================================

// Dessine la grille de fond et les cadres futuristes
void drawHudBackground(uint16_t colPrimary, uint16_t colMid, uint16_t colDim, uint16_t colAccent) {
    gfx->fillScreen(COL_BG);

    // Grille tactique subtile
    for (int x = 0; x < SCREEN_W; x += 40) {
        gfx->drawFastVLine(x, 0, SCREEN_H, COL_GRID);
    }
    for (int y = 0; y < SCREEN_H; y += 40) {
        gfx->drawFastHLine(0, y, SCREEN_W, COL_GRID);
    }

    // Bandeau supérieur coupé style Sci-Fi
    gfx->drawFastHLine(15, 10, 770, colMid);
    gfx->drawFastHLine(15, 38, 230, colPrimary);
    gfx->drawLine(245, 38, 265, 22, colPrimary);
    gfx->drawFastHLine(265, 22, 270, colPrimary);
    gfx->drawLine(535, 22, 555, 38, colPrimary);
    gfx->drawFastHLine(555, 38, 230, colPrimary);

    // Coins renforcés
    gfx->fillRect(15, 10, 12, 4, colAccent);
    gfx->fillRect(773, 10, 12, 4, colAccent);
    gfx->fillRect(15, 466, 12, 4, colAccent);
    gfx->fillRect(773, 466, 12, 4, colAccent);
    gfx->drawFastHLine(15, 470, 770, colDim);

    // Titres du bandeau supérieur
    gfx->setTextSize(2);
    gfx->setTextColor(colPrimary);
    gfx->setCursor(25, 16);
    gfx->print("J.A.R.V.I.S.");

    gfx->setTextSize(1);
    gfx->setTextColor(colAccent);
    gfx->setCursor(180, 20);
    gfx->print("MK-LXXXV");

    // Statut central haut
    gfx->setTextSize(1);
    gfx->setTextColor(colPrimary);
    gfx->setCursor(325, 11);
    if (combatMode) {
        gfx->print("!! COMBAT PROTOCOL ENGAGED !!");
    } else {
        gfx->print("STARK INDUSTRIES // AI CORE");
    }

    // Compteur FPS et mémoire à droite
    gfx->setTextColor(colMid);
    gfx->setCursor(575, 18);
    gfx->printf("FPS: %.0f | PSRAM: %dMB", currentFps, ESP.getPsramSize() / (1024 * 1024));
}

// Dessine le Réacteur Arc central animé (X=400, Y=248)
void drawArcReactor(int16_t cx, int16_t cy, uint16_t colBright, uint16_t colPrimary, uint16_t colMid, uint16_t colDim, uint16_t colAccent) {
    // 1. Cercle de boussole externe fixe + graduations
    gfx->drawCircle(cx, cy, 184, colDim);
    gfx->drawCircle(cx, cy, 178, colDim);
    for (int deg = 0; deg < 360; deg += 15) {
        float rad = deg * DEGTORAD;
        int rIn = (deg % 45 == 0) ? 170 : 175;
        int x0 = cx + cosf(rad) * rIn;
        int y0 = cy + sinf(rad) * rIn;
        int x1 = cx + cosf(rad) * 183;
        int y1 = cy + sinf(rad) * 183;
        gfx->drawLine(x0, y0, x1, y1, (deg % 90 == 0) ? colAccent : colMid);
    }

    // 2. Anneau 1 (Externe) : 4 grands arcs rotatifs (sens horaire)
    for (int i = 0; i < 4; i++) {
        float start = fmodf(angleRing1 + i * 90.0f, 360.0f);
        float end = start + 52.0f;
        gfx->fillArc(cx, cy, 162, 150, start, end, colPrimary);
        // Petit accent doré/orange entre les segments
        float accStart = fmodf(end + 6.0f, 360.0f);
        gfx->fillArc(cx, cy, 160, 152, accStart, accStart + 10.0f, colAccent);
    }

    // 3. Anneau 2 (Bobines de Palladium du Réacteur Arc) : 10 segments + rotation inverse
    gfx->drawCircle(cx, cy, 142, colMid);
    for (int i = 0; i < 10; i++) {
        float start = fmodf(angleRing2 + i * 36.0f + 360.0f, 360.0f);
        float end = start + 22.0f;
        gfx->fillArc(cx, cy, 138, 118, start, end, colMid);
        gfx->drawArc(cx, cy, 139, 117, start, end, colBright);
    }

    // 4. Anneau 3 (Gyroscope rapide interne) : 3 arcs fins + indicateurs
    for (int i = 0; i < 3; i++) {
        float start = fmodf(angleRing3 + i * 120.0f, 360.0f);
        gfx->fillArc(cx, cy, 108, 100, start, start + 75.0f, colPrimary);
        gfx->fillArc(cx, cy, 94, 90, fmodf(360.0f - start + i * 120.0f, 360.0f),
                     fmodf(360.0f - start + i * 120.0f, 360.0f) + 50.0f, colAccent);
    }

    // 5. Cœur d'énergie pulsant (Respiration sinusoïdale)
    float pulse = (sinf(pulsePhase) + 1.0f) * 0.5f; // 0.0 -> 1.0
    int coreR = 52 + (int)(pulse * 12.0f);

    gfx->fillCircle(cx, cy, 76, COL_BG);
    gfx->drawCircle(cx, cy, 76, colMid);
    gfx->fillCircle(cx, cy, coreR + 10, colDim);
    gfx->fillCircle(cx, cy, coreR, colMid);
    gfx->fillCircle(cx, cy, coreR - 14, colPrimary);
    gfx->fillCircle(cx, cy, coreR - 28, colBright);

    // Structure géométrique triangulaire du cœur (Style Iron Man 2 / Avengers)
    for (int i = 0; i < 3; i++) {
        float a1 = (angleRing1 * 0.5f + i * 120.0f) * DEGTORAD;
        float a2 = (angleRing1 * 0.5f + (i + 1) * 120.0f) * DEGTORAD;
        int x1 = cx + cosf(a1) * 68;
        int y1 = cy + sinf(a1) * 68;
        int x2 = cx + cosf(a2) * 68;
        int y2 = cy + sinf(a2) * 68;
        gfx->drawLine(x1, y1, x2, y2, colBright);
    }

    // Réticule central et texte du noyau
    gfx->drawFastHLine(cx - 25, cy, 50, COL_BG);
    gfx->drawFastVLine(cx, cy - 25, 50, COL_BG);

    gfx->setTextSize(1);
    gfx->setTextColor(COL_BG);
    gfx->setCursor(cx - 18, cy - 3);
    gfx->print(combatMode ? "MAX" : "ARC");

    // Indicateurs d'angle autour du réacteur
    gfx->setTextColor(colAccent);
    gfx->setCursor(cx - 24, cy + 190);
    gfx->printf("HDG %03d", (int)angleRing1);
}

// Dessine le panneau gauche : Télémétrie de l'armure & Radar tactique
void drawLeftPanel(uint16_t colPrimary, uint16_t colMid, uint16_t colDim, uint16_t colAccent) {
    // Cadre télémétrie haut-gauche
    gfx->drawRect(15, 52, 185, 195, colDim);
    gfx->fillRect(15, 52, 185, 18, colDim);
    gfx->setTextSize(1);
    gfx->setTextColor(colPrimary);
    gfx->setCursor(22, 57);
    gfx->print("ARMOR TELEMETRY");

    const char *labels[4] = {"REACTOR", "SHIELDS", "THRUST", "NANOTECH"};
    int baseVals[4] = {98, 86, 74, 92};

    for (int i = 0; i < 4; i++) {
        int y = 80 + i * 40;
        int val = baseVals[i] + (int)(sinf(pulsePhase + i * 1.3f) * 6.0f);
        if (combatMode) val = min(100, val + 10);

        gfx->setTextColor(colPrimary);
        gfx->setCursor(24, y);
        gfx->printf("%s", labels[i]);
        gfx->setTextColor(colAccent);
        gfx->setCursor(160, y);
        gfx->printf("%d%%", val);

        // Barre de progression segmentée
        gfx->drawRect(24, y + 12, 164, 12, colMid);
        int fillW = (val * 158) / 100;
        gfx->fillRect(27, y + 15, fillW, 6, (i == 0 || combatMode) ? colAccent : colPrimary);
    }

    // Cadre Radar Tactique bas-gauche (Centre X=107, Y=360, R=72)
    int rcx = 107;
    int rcy = 362;
    int rr = 72;
    gfx->drawRect(15, 260, 185, 198, colDim);
    gfx->fillRect(15, 260, 185, 18, colDim);
    gfx->setTextColor(colPrimary);
    gfx->setCursor(22, 265);
    gfx->print("TACTICAL RADAR");

    gfx->drawCircle(rcx, rcy, rr, colMid);
    gfx->drawCircle(rcx, rcy, rr * 2 / 3, colDim);
    gfx->drawCircle(rcx, rcy, rr / 3, colDim);
    gfx->drawFastHLine(rcx - rr, rcy, rr * 2, colDim);
    gfx->drawFastVLine(rcx, rcy - rr, rr * 2, colDim);

    // Balayage radar rotatif
    float rRad = angleRadar * DEGTORAD;
    int sx = rcx + cosf(rRad) * rr;
    int sy = rcy + sinf(rRad) * rr;
    gfx->drawLine(rcx, rcy, sx, sy, colPrimary);
    gfx->fillArc(rcx, rcy, rr - 2, 4, fmodf(angleRadar - 25.0f + 360.0f, 360.0f), angleRadar, colDim);

    // Cibles simulées sur le radar
    int bx1 = rcx + cosf(pulsePhase * 0.3f) * 42;
    int by1 = rcy + sinf(pulsePhase * 0.3f) * 35;
    gfx->fillCircle(bx1, by1, 3, colAccent);
    gfx->drawCircle(bx1, by1, 7, colAccent);

    int bx2 = rcx - cosf(pulsePhase * 0.2f + 1.0f) * 50;
    int by2 = rcy + sinf(pulsePhase * 0.2f + 1.0f) * 25;
    gfx->fillCircle(bx2, by2, 2, colPrimary);
}

// Dessine le panneau droit : Analyseur Vocal J.A.R.V.I.S., Logs & Boutons Tactiles
void drawRightPanel(uint16_t colBright, uint16_t colPrimary, uint16_t colMid, uint16_t colDim, uint16_t colAccent) {
    // 1. Synthétiseur Vocal (Onde audio temps réel)
    gfx->drawRect(600, 52, 185, 130, colDim);
    gfx->fillRect(600, 52, 185, 18, colDim);
    gfx->setTextSize(1);
    gfx->setTextColor(colPrimary);
    gfx->setCursor(608, 57);
    gfx->print("VOICE MODULATOR");

    int waveCenterY = 122;
    for (int i = 0; i < 16; i++) {
        int barX = 612 + i * 10;
        // Hauteur modulée par plusieurs sinusoïdes pour simuler la voix de J.A.R.V.I.S.
        float env = sinf((i / 15.0f) * M_PI); // Enveloppe plus forte au centre
        float wave = fabs(sinf(pulsePhase * 3.2f + i * 0.55f) * cosf(pulsePhase * 1.7f - i * 0.3f));
        int barH = 4 + (int)(wave * env * 46.0f);

        uint16_t bCol = (barH > 28) ? colAccent : ((barH > 16) ? colBright : colPrimary);
        gfx->fillRect(barX, waveCenterY - barH, 6, barH * 2, bCol);
    }

    // 2. Console de Diagnostic Temps Réel
    gfx->drawRect(600, 194, 185, 165, colDim);
    gfx->fillRect(600, 194, 185, 18, colDim);
    gfx->setTextColor(colPrimary);
    gfx->setCursor(608, 199);
    gfx->print("SYSTEM DIAGNOSTICS");

    for (int i = 0; i < 7; i++) {
        int logIdx = (i + (frameCount / 25)) % 8;
        gfx->setTextColor((i == 6) ? colBright : colMid);
        gfx->setCursor(606, 220 + i * 18);
        gfx->print(jarvisLogs[logIdx]);
    }

    // 3. Boutons Tactiles Interactifs (Bas-Droite)
    // Bouton 1 : Basculer Mode NORMAL / COMBAT OVERDRIVE
    gfx->fillRoundRect(600, 372, 185, 40, 6, combatMode ? colAccent : colDim);
    gfx->drawRoundRect(600, 372, 185, 40, 6, colBright);
    gfx->setTextSize(2);
    gfx->setTextColor(combatMode ? COL_BG : colBright);
    gfx->setCursor(622, 384);
    gfx->print(combatMode ? "MODE: COMBAT" : "MODE: NORMAL");

    // Bouton 2 : Activer/Désactiver le Scanner Laser
    gfx->fillRoundRect(600, 420, 185, 38, 6, scanActive ? colPrimary : colDim);
    gfx->drawRoundRect(600, 420, 185, 38, 6, colPrimary);
    gfx->setTextSize(2);
    gfx->setTextColor(scanActive ? COL_BG : colPrimary);
    gfx->setCursor(626, 431);
    gfx->print(scanActive ? "SCAN: ON" : "SCAN: OFF");
}

// Dessine le réticule de verrouillage cible sous le doigt et le scanner
void drawOverlays(uint16_t colBright, uint16_t colPrimary, uint16_t colAccent) {
    // Ligne de scan laser horizontale si activée
    if (scanActive) {
        int sy = (int)scanLineY;
        gfx->drawFastHLine(205, sy, 390, colBright);
        gfx->drawFastHLine(205, sy - 1, 390, colPrimary);
        gfx->drawFastHLine(205, sy + 1, 390, colPrimary);
    }

    // Réticule de visée interactif si l'utilisateur touche la zone centrale
    if (hasTouchTarget) {
        int r = 28 + (int)(sinf(pulsePhase * 4.0f) * 5.0f);
        gfx->drawCircle(targetX, targetY, r, colAccent);
        gfx->drawCircle(targetX, targetY, 6, colBright);
        gfx->drawFastHLine(targetX - r - 10, targetY, 14, colAccent);
        gfx->drawFastHLine(targetX + r - 3, targetY, 14, colAccent);
        gfx->drawFastVLine(targetX, targetY - r - 10, 14, colAccent);
        gfx->drawFastVLine(targetX, targetY + r - 3, 14, colAccent);

        gfx->setTextSize(1);
        gfx->setTextColor(colBright);
        gfx->setCursor(targetX + 16, targetY - 18);
        gfx->printf("LOCK [%d,%d]", targetX, targetY);
    }
}

// =============================================================================
// 5. GESTION TACTILE (GT911)
// =============================================================================
void handleTouch() {
    tp.read();
    if (tp.isTouched) {
        int16_t tx = tp.points[0].x;
        int16_t ty = tp.points[0].y;
        uint32_t now = millis();

        // Clic sur le bouton MODE (X: 600..785, Y: 372..412) ou au centre du Réacteur Arc
        float distCenter = hypotf(tx - 400, ty - 248);
        if (((tx >= 600 && tx <= 785 && ty >= 372 && ty <= 412) || distCenter < 65.0f) &&
            (now - lastTouchToggleMs > 350)) {
            combatMode = !combatMode;
            lastTouchToggleMs = now;
        }
        // Clic sur le bouton SCAN (X: 600..785, Y: 420..458)
        else if (tx >= 600 && tx <= 785 && ty >= 420 && ty <= 458 && (now - lastTouchToggleMs > 350)) {
            scanActive = !scanActive;
            lastTouchToggleMs = now;
        }
        // Toucher ailleurs : déplace le réticule de verrouillage holographique
        else {
            targetX = constrain(tx, 210, 590);
            targetY = constrain(ty, 55, 450);
            hasTouchTarget = true;
        }
    }
}

// =============================================================================
// 6. SETUP & LOOP PRINCIPALE
// =============================================================================
void setup() {
    Serial.begin(115200);

    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);

    // Initialisation du double buffer en PSRAM (ou fallback direct)
    if (!canvas->begin()) {
        Serial.println("Fallback sur rgbDisplay direct");
        gfx = rgbDisplay;
        gfx->begin();
    }

    // Initialisation du tactile capacitif GT911
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    tp.begin();
    tp.setRotation(ROTATION_NORMAL);

    lastFpsMs = millis();
    Serial.println("J.A.R.V.I.S. HUD Initialisé !");
}

void loop() {
    // 1. Lecture des entrées tactiles
    handleTouch();

    // 2. Mise à jour des vitesses d'animation (plus rapide en mode Combat)
    float speedMult = combatMode ? 2.4f : 1.0f;
    angleRing1 = fmodf(angleRing1 + 1.8f * speedMult, 360.0f);
    angleRing2 = fmodf(angleRing2 - 1.2f * speedMult + 360.0f, 360.0f);
    angleRing3 = fmodf(angleRing3 + 3.2f * speedMult, 360.0f);
    angleRadar = fmodf(angleRadar + 4.0f * speedMult, 360.0f);
    pulsePhase += 0.12f * speedMult;

    if (scanActive) {
        scanLineY += 5.0f * speedMult;
        if (scanLineY > 445.0f) scanLineY = 50.0f;
    }

    // 3. Sélection de la palette selon le mode (Cyan J.A.R.V.I.S. vs Rouge Combat)
    uint16_t colBright  = combatMode ? RED_BRIGHT  : CYAN_BRIGHT;
    uint16_t colPrimary = combatMode ? RED_NEON    : CYAN_NEON;
    uint16_t colMid     = combatMode ? RED_MID     : CYAN_MID;
    uint16_t colDim     = combatMode ? RED_DIM     : CYAN_DIM;
    uint16_t colAccent  = combatMode ? ORANGE_WARN : GOLD_STARK;

    // 4. Rendu complet de la scène dans le buffer PSRAM
    drawHudBackground(colPrimary, colMid, colDim, colAccent);
    drawLeftPanel(colPrimary, colMid, colDim, colAccent);
    drawRightPanel(colBright, colPrimary, colMid, colDim, colAccent);
    drawArcReactor(400, 248, colBright, colPrimary, colMid, colDim, colAccent);
    drawOverlays(colBright, colPrimary, colAccent);

    // 5. Envoi instantané de l'image complète à l'écran (Zéro Flickering)
    gfx->flush();

    // 6. Calcul des FPS
    frameCount++;
    uint32_t now = millis();
    if (now - lastFpsMs >= 1000) {
        currentFps = (frameCount * 1000.0f) / (now - lastFpsMs);
        frameCount = 0;
        lastFpsMs = now;
    }
}