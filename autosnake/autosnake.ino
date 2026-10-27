// Auto-playing Snake for Arduino Uno + 2.4" TFT LCD shield.
//
// No buttons, no menus: the game starts playing itself instantly, always
// wins, shows "YOU WIN!" for 5 seconds, then restarts automatically.
//
// Hardware: Arduino Uno (or compatible) with a 2.4" TFT LCD shield
//           (MCUFRIEND-style, ILI9341 / ILI9325 / etc.). Just plug the
//           shield onto the Uno, no extra wiring.
//
// Libraries (Arduino IDE -> Sketch -> Include Library -> Manage Libraries):
//   - "Adafruit GFX Library" by Adafruit
//   - "MCUFRIEND_kbv" by David Prentice
//
// Then select Board "Arduino Uno", the right port, and Upload.

#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

#include "snake_core.h"

MCUFRIEND_kbv tft;
autosnake::Game game;

// ---- Layout (landscape 320x240) ----
static const int CELL = 18;
static const int OX = 16;  // (320 - 16*18) / 2
static const int OY = 22;  // rows 0..20 are the HUD strip
static const int FIELD_W = autosnake::GRID_W * CELL;  // 288
static const int FIELD_H = autosnake::GRID_H * CELL;  // 216

// ---- Colors (RGB565) ----
static const uint16_t COL_BG = 0x0000;
static const uint16_t COL_BODY = 0x07E0;    // green
static const uint16_t COL_HEAD = 0xFFE0;    // yellow
static const uint16_t COL_FOOD = 0xF800;    // red
static const uint16_t COL_BORDER = 0x07FF;  // cyan
static const uint16_t COL_TEXT = 0xFFFF;    // white

void drawCell(uint8_t gx, uint8_t gy, uint16_t color) {
  tft.fillRect(OX + gx * CELL + 1, OY + gy * CELL + 1, CELL - 2, CELL - 2,
               color);
}

void eraseCell(uint8_t gx, uint8_t gy) {
  tft.fillRect(OX + gx * CELL, OY + gy * CELL, CELL, CELL, COL_BG);
}

void drawHUD() {
  tft.fillRect(0, 0, 320, OY - 1, COL_BG);
  tft.setTextColor(COL_TEXT);
  tft.setTextSize(2);
  tft.setCursor(16, 3);
  tft.print("AUTO SNAKE");
  tft.setCursor(216, 3);
  tft.print("LEN ");
  tft.print((int)game.len);
}

void drawBoard() {
  tft.fillScreen(COL_BG);
  tft.drawRect(OX - 1, OY - 1, FIELD_W + 2, FIELD_H + 2, COL_BORDER);
  for (uint8_t k = 0; k < game.len; k++) {
    uint8_t pos = game.cycPos[game.bodyAt(k)];
    drawCell(pos % autosnake::GRID_W, pos / autosnake::GRID_W,
             k + 1 == game.len ? COL_HEAD : COL_BODY);
  }
  drawCell(game.foodX(), game.foodY(), COL_FOOD);
  drawHUD();
}

void showWin() {
  tft.fillScreen(COL_BG);
  tft.setTextColor(COL_BODY);
  tft.setTextSize(5);
  tft.setCursor(40, 90);
  tft.print("YOU WIN!");
  tft.setTextColor(COL_TEXT);
  tft.setTextSize(2);
  tft.setCursor(106, 150);
  tft.print("SCORE ");
  tft.print(autosnake::NCELLS);
}

void resetAndDraw() {
  game.reset();
  game.spawnFood((uint16_t)random(game.freeCount()));
  drawBoard();
}

void setup() {
  Serial.begin(9600);
  uint16_t id = tft.readID();
  if (id == 0xD3D3) id = 0x9341;  // write-only shield quirk
  tft.begin(id);
  tft.setRotation(1);  // landscape 320x240
  randomSeed(analogRead(A5));
  resetAndDraw();
}

void loop() {
  // Adaptive speed: sprint while the food is far away, slow down when
  // close so each catch stays watchable. Speed never affects the AI,
  // which provably cannot die (see snake_core.h).
  uint8_t dist = autosnake::Game::cycDist(game.headC(), game.foodC);
  int stepMs = dist > 48 ? 5 : (dist > 16 ? 10 : 25);
  autosnake::StepResult r = game.step();
  if (r.event == autosnake::EV_DEAD) {  // defensive; the AI never dies
    Serial.println("AI died? restarting");
    resetAndDraw();
    return;
  }
  if (!r.grew) eraseCell(r.tx, r.ty);
  drawCell(r.ohx, r.ohy, COL_BODY);
  drawCell(r.hx, r.hy, COL_HEAD);
  if (r.event == autosnake::EV_ATE) {
    game.spawnFood((uint16_t)random(game.freeCount()));
    drawCell(game.foodX(), game.foodY(), COL_FOOD);
    drawHUD();
  } else if (r.event == autosnake::EV_WIN) {
    drawHUD();
    delay(800);
    showWin();
    delay(5000);
    resetAndDraw();
    return;
  }
  delay(stepMs);
}
