// TFT Arcade - 3 touch games for Arduino Uno + 2.4" MCUFRIEND TFT shield.
// Games: Tic-Tac-Toe (vs CPU), Breakout, Snake. Portrait 240x320.
// Touch auto-calibration is stored in EEPROM (see CALIBRATE button in menu).
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>
#include <EEPROM.h>

MCUFRIEND_kbv tft;
uint16_t gID = 0;

// MCUFRIEND_kbv 3.x renamed classic color macros with a TFT_ prefix.
#define BLACK TFT_BLACK
#define NAVY TFT_NAVY
#define DARKGREY TFT_DARKGREY
#define GREEN TFT_GREEN
#define CYAN TFT_CYAN
#define RED TFT_RED
#define MAGENTA TFT_MAGENTA
#define YELLOW TFT_YELLOW
#define WHITE TFT_WHITE
#define ORANGE TFT_ORANGE

// ---- Touch pins: standard MCUFRIEND Uno shield wiring ----
#define XP 8
#define XM A2
#define YP A3
#define YM 9
#define MINPRESSURE 200
#define MAXPRESSURE 1000
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// ---- Screen layout (portrait) ----
#define SCR_W 240
#define SCR_H 320
#define HDR_H 30
#define MENU_X 172

// ---- Touch calibration (2-point, stored in EEPROM) ----
#define CAL_MAGIC 0xCA
#define CAL_TX0 20
#define CAL_TY0 20
#define CAL_TX1 219
#define CAL_TY1 299
struct CalData {
  uint8_t magic;
  int16_t x0, y0, x1, y1;  // raw readings at the two screen targets
};
CalData cal;

// ================= touch helpers =================
bool readTouchRaw(int &rx, int &ry) {
  TSPoint p = ts.getPoint();
  pinMode(XM, OUTPUT);  // restore TFT control pins shared with touch
  pinMode(YP, OUTPUT);
  if (p.z < MINPRESSURE || p.z > MAXPRESSURE) return false;
  rx = p.x;
  ry = p.y;
  return true;
}

bool readTouch(int &sx, int &sy) {
  int rx, ry;
  if (!readTouchRaw(rx, ry)) return false;
  long x = map((long)rx, cal.x0, cal.x1, CAL_TX0, CAL_TX1);
  long y = map((long)ry, cal.y0, cal.y1, CAL_TY0, CAL_TY1);
  sx = constrain((int)x, 0, SCR_W - 1);
  sy = constrain((int)y, 0, SCR_H - 1);
  return true;
}

void waitRelease() {
  int rx, ry;
  while (readTouchRaw(rx, ry)) delay(20);
  delay(60);
}

// ================= UI helpers =================
void centerText(const __FlashStringHelper *s, int y, uint8_t size,
                uint16_t color, uint16_t bg) {
  int len = strlen_P((const char *)s);
  tft.setTextSize(size);
  tft.setTextColor(color, bg);
  tft.setCursor((SCR_W - len * 6 * size) / 2, y);
  tft.print(s);
}

void drawHeader(const __FlashStringHelper *title) {
  tft.fillRect(0, 0, SCR_W, HDR_H, NAVY);
  tft.fillRect(MENU_X, 3, SCR_W - MENU_X - 3, HDR_H - 6, RED);
  tft.setTextSize(2);
  tft.setTextColor(WHITE, NAVY);
  tft.setCursor(6, 8);
  tft.print(title);
  tft.setTextColor(WHITE, RED);
  tft.setCursor(MENU_X + 9, 8);
  tft.print(F("MENU"));
}

bool inMenu(int sx, int sy) { return sx >= MENU_X - 4 && sy < HDR_H + 4; }

void drawTarget(int x, int y) {
  tft.drawLine(x - 10, y, x + 10, y, WHITE);
  tft.drawLine(x, y - 10, x, y + 10, WHITE);
  tft.drawCircle(x, y, 4, RED);
}

// ================= calibration =================
bool readStableRaw(int &ax, int &ay) {
  long sx = 0, sy = 0;
  for (int i = 0; i < 8; i++) {
    int rx, ry;
    if (!readTouchRaw(rx, ry)) return false;
    sx += rx;
    sy += ry;
    delay(10);
  }
  ax = (int)(sx / 8);
  ay = (int)(sy / 8);
  return true;
}

void runCalibration() {
  tft.fillScreen(BLACK);
  centerText(F("TOUCH CALIBRATION"), 60, 2, YELLOW, BLACK);
  centerText(F("Tap each + target"), 100, 1, WHITE, BLACK);
  int16_t raw[2][2];
  const int tx[2] = {CAL_TX0, CAL_TX1};
  const int ty[2] = {CAL_TY0, CAL_TY1};
  for (int i = 0; i < 2; i++) {
    drawTarget(tx[i], ty[i]);
    int ax = 0, ay = 0;
    for (;;) {
      int rx, ry;
      if (readTouchRaw(rx, ry)) {
        if (readStableRaw(ax, ay)) break;
      }
      delay(20);
    }
    raw[i][0] = ax;
    raw[i][1] = ay;
    tft.fillCircle(tx[i], ty[i], 12, GREEN);
    delay(250);
    waitRelease();
    tft.fillRect(tx[i] - 13, ty[i] - 13, 27, 27, BLACK);
  }
  cal.magic = CAL_MAGIC;
  cal.x0 = raw[0][0];
  cal.y0 = raw[0][1];
  cal.x1 = raw[1][0];
  cal.y1 = raw[1][1];
  EEPROM.put(0, cal);
  centerText(F("Saved!"), 150, 3, GREEN, BLACK);
  delay(800);
}

void loadCal() {
  EEPROM.get(0, cal);
  if (cal.magic != CAL_MAGIC) runCalibration();
}

// ================= menu =================
#define BTN_X 20
#define BTN_W 200
#define BTN_H 56
const int btnY[3] = {70, 140, 210};

void menuButton(int i, const __FlashStringHelper *label, uint16_t color) {
  tft.fillRoundRect(BTN_X, btnY[i], BTN_W, BTN_H, 8, color);
  int len = strlen_P((const char *)label);
  tft.setTextSize(2);
  tft.setTextColor(BLACK, color);
  tft.setCursor((SCR_W - len * 12) / 2, btnY[i] + 20);
  tft.print(label);
}

void showMenu() {
  tft.fillScreen(BLACK);
  centerText(F("ARDUINO ARCADE"), 22, 2, YELLOW, BLACK);
  tft.setTextSize(1);
  tft.setTextColor(GREEN, BLACK);
  tft.setCursor(66, 46);
  tft.print(F("TFT driver: 0x"));
  tft.print(gID, HEX);
  menuButton(0, F("TIC-TAC-TOE"), CYAN);
  menuButton(1, F("BREAKOUT"), GREEN);
  menuButton(2, F("SNAKE"), MAGENTA);
  tft.fillRoundRect(70, 290, 100, 24, 6, DARKGREY);
  tft.setTextSize(1);
  tft.setTextColor(WHITE, DARKGREY);
  tft.setCursor(88, 298);
  tft.print(F("CALIBRATE"));
}

// ================= game 1: tic-tac-toe =================
uint8_t tttBoard[9];
#define TTT_CELL 64
#define TTT_X0 24
#define TTT_Y0 100
int tttScoreYou = 0, tttScoreCpu = 0;

int tttWinner() {
  const uint8_t L[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6},
                            {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};
  for (int i = 0; i < 8; i++) {
    uint8_t a = tttBoard[L[i][0]];
    if (a != 0 && a == tttBoard[L[i][1]] && a == tttBoard[L[i][2]]) return a;
  }
  for (int i = 0; i < 9; i++)
    if (tttBoard[i] == 0) return 0;
  return 3;
}

int tttFindWin(uint8_t who) {
  for (int i = 0; i < 9; i++) {
    if (tttBoard[i] == 0) {
      tttBoard[i] = who;
      int w = tttWinner();
      tttBoard[i] = 0;
      if (w == who) return i;
    }
  }
  return -1;
}

int tttBestMove() {
  int m = tttFindWin(2);
  if (m >= 0) return m;
  m = tttFindWin(1);
  if (m >= 0) return m;
  if (tttBoard[4] == 0) return 4;
  const uint8_t order[8] = {0, 2, 6, 8, 1, 3, 5, 7};
  uint8_t s = random(4);
  for (int k = 0; k < 4; k++) {
    int i = order[(s + k) % 4];
    if (tttBoard[i] == 0) return i;
  }
  s = random(4);
  for (int k = 0; k < 4; k++) {
    int i = order[4 + (s + k) % 4];
    if (tttBoard[i] == 0) return i;
  }
  return -1;
}

void tttDrawMark(int cell, uint8_t who) {
  int x = TTT_X0 + (cell % 3) * TTT_CELL;
  int y = TTT_Y0 + (cell / 3) * TTT_CELL;
  if (who == 1) {
    for (int o = -1; o <= 1; o++) {
      tft.drawLine(x + 14, y + 14 + o, x + TTT_CELL - 14, y + TTT_CELL - 14 + o, YELLOW);
      tft.drawLine(x + TTT_CELL - 14, y + 14 + o, x + 14, y + TTT_CELL - 14 + o, YELLOW);
    }
  } else {
    int cx = x + TTT_CELL / 2, cy = y + TTT_CELL / 2;
    for (int r = 20; r <= 22; r++) tft.drawCircle(cx, cy, r, CYAN);
  }
}

void tttDrawGrid() {
  tft.fillRect(TTT_X0, TTT_Y0, TTT_CELL * 3, TTT_CELL * 3, BLACK);
  for (int i = 1; i < 3; i++) {
    tft.drawLine(TTT_X0 + i * TTT_CELL, TTT_Y0, TTT_X0 + i * TTT_CELL,
                 TTT_Y0 + TTT_CELL * 3, WHITE);
    tft.drawLine(TTT_X0, TTT_Y0 + i * TTT_CELL, TTT_X0 + TTT_CELL * 3,
                 TTT_Y0 + i * TTT_CELL, WHITE);
  }
}

void tttStatus(const __FlashStringHelper *s, uint16_t color) {
  tft.fillRect(0, 32, SCR_W, 22, BLACK);
  centerText(s, 36, 2, color, BLACK);
}

void tttScores() {
  tft.fillRect(0, 56, SCR_W, 16, BLACK);
  tft.setTextSize(2);
  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(52, 56);
  tft.print(F("YOU "));
  tft.print(tttScoreYou);
  tft.print(F(" : "));
  tft.print(tttScoreCpu);
  tft.print(F(" CPU"));
}

void tttNewRound() {
  for (int i = 0; i < 9; i++) tttBoard[i] = 0;
  drawHeader(F("TIC-TAC-TOE"));
  tttScores();
  tttStatus(F("Your move (X)"), GREEN);
  tttDrawGrid();
}

int tttCellAt(int sx, int sy) {
  int c = (sx - TTT_X0) / TTT_CELL;
  int r = (sy - TTT_Y0) / TTT_CELL;
  if (c < 0 || c > 2 || r < 0 || r > 2) return -1;
  return r * 3 + c;
}

void tttResult(const __FlashStringHelper *s, uint16_t color) {
  tft.fillRoundRect(30, 150, 180, 68, 8, NAVY);
  tft.drawRoundRect(30, 150, 180, 68, 8, WHITE);
  centerText(s, 160, 2, color, NAVY);
  centerText(F("tap to replay"), 188, 1, WHITE, NAVY);
}

void playTicTacToe() {
  tttScoreYou = 0;
  tttScoreCpu = 0;
  tttNewRound();
  bool gameOver = false;
  while (true) {
    int sx, sy;
    if (!readTouch(sx, sy)) {
      delay(30);
      continue;
    }
    if (inMenu(sx, sy)) return;
    if (gameOver) {
      waitRelease();
      tttNewRound();
      gameOver = false;
      continue;
    }
    int cell = tttCellAt(sx, sy);
    if (cell < 0 || tttBoard[cell] != 0) {
      waitRelease();
      continue;
    }
    tttBoard[cell] = 1;
    tttDrawMark(cell, 1);
    int w = tttWinner();
    if (w == 1) {
      tttScoreYou++;
      tttScores();
      tttResult(F("YOU WIN!"), YELLOW);
      gameOver = true;
    } else if (w == 3) {
      tttResult(F("DRAW!"), WHITE);
      gameOver = true;
    } else {
      tttStatus(F("CPU thinks..."), CYAN);
      delay(400);
      int m = tttBestMove();
      if (m >= 0) {
        tttBoard[m] = 2;
        tttDrawMark(m, 2);
      }
      w = tttWinner();
      if (w == 2) {
        tttScoreCpu++;
        tttScores();
        tttResult(F("CPU WINS!"), RED);
        gameOver = true;
      } else if (w == 3) {
        tttResult(F("DRAW!"), WHITE);
        gameOver = true;
      } else {
        tttStatus(F("Your move (X)"), GREEN);
      }
    }
    waitRelease();
  }
}

// ================= game 2: breakout =================
#define BR_COLS 6
#define BR_ROWS 4
#define BR_W 37
#define BR_H 14
#define BR_GAP 2
#define BR_X0 4
#define BR_Y0 70
#define BR_TOP 52
#define PADDLE_W 52
#define PADDLE_H 8
#define PADDLE_Y 296
#define BALL_R 4

uint8_t brBricks[BR_COLS * BR_ROWS];
int brScore, brLives, brLevel, brLeft;
int paddleX, paddleTarget;
float ballX, ballY, ballVX, ballVY, ballSpeed;
const uint16_t brColors[BR_ROWS] = {RED, ORANGE, YELLOW, GREEN};

void brInfo() {
  tft.fillRect(0, 32, SCR_W, 16, BLACK);
  tft.setTextSize(1);
  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(6, 36);
  tft.print(F("SCORE "));
  tft.print(brScore);
  tft.print(F("  LV "));
  tft.print(brLevel);
  tft.setCursor(180, 36);
  tft.print(F("BALLS "));
  tft.print(brLives);
}

void brBuildBricks() {
  brLeft = BR_COLS * BR_ROWS;
  for (int r = 0; r < BR_ROWS; r++) {
    for (int c = 0; c < BR_COLS; c++) {
      brBricks[r * BR_COLS + c] = 1;
      tft.fillRect(BR_X0 + c * (BR_W + BR_GAP), BR_Y0 + r * (BR_H + BR_GAP),
                   BR_W, BR_H, brColors[r]);
    }
  }
}

void brResetBall() {
  paddleX = SCR_W / 2;
  paddleTarget = paddleX;
  tft.fillRect(0, PADDLE_Y, SCR_W, PADDLE_H + 2, BLACK);
  tft.fillRect(paddleX - PADDLE_W / 2, PADDLE_Y, PADDLE_W, PADDLE_H, CYAN);
  ballX = paddleX;
  ballY = PADDLE_Y - BALL_R - 1;
  ballVX = (random(2) ? 1 : -1) * ballSpeed * 0.6;
  ballVY = -ballSpeed * 0.8;
  tft.fillCircle((int)ballX, (int)ballY, BALL_R, WHITE);
}

void brOverlay(const __FlashStringHelper *l1, const __FlashStringHelper *l2,
               uint16_t color) {
  tft.fillRoundRect(30, 150, 180, 68, 8, NAVY);
  tft.drawRoundRect(30, 150, 180, 68, 8, WHITE);
  centerText(l1, 160, 2, color, NAVY);
  centerText(l2, 188, 1, WHITE, NAVY);
}

// tap to continue; returns false if MENU tapped
bool brWaitTap() {
  waitRelease();
  while (true) {
    int sx, sy;
    if (readTouch(sx, sy)) {
      if (inMenu(sx, sy)) return false;
      waitRelease();
      return true;
    }
    delay(30);
  }
}

void playBreakout() {
  drawHeader(F("BREAKOUT"));
  brScore = 0;
  brLives = 3;
  brLevel = 1;
  ballSpeed = 2.2;
  brInfo();
  brBuildBricks();
  brResetBall();
  unsigned long lastStep = millis();

  while (true) {
    int sx, sy;
    if (readTouch(sx, sy)) {
      if (inMenu(sx, sy)) return;
      paddleTarget = constrain(sx, PADDLE_W / 2 + 1, SCR_W - PADDLE_W / 2 - 1);
    }
    if (millis() - lastStep < 20) continue;
    lastStep = millis();

    // move paddle
    if (paddleX != paddleTarget) {
      tft.fillRect(0, PADDLE_Y, SCR_W, PADDLE_H, BLACK);
      paddleX = paddleTarget;
      tft.fillRect(paddleX - PADDLE_W / 2, PADDLE_Y, PADDLE_W, PADDLE_H, CYAN);
    }

    // move ball
    tft.fillCircle((int)ballX, (int)ballY, BALL_R, BLACK);
    ballX += ballVX;
    ballY += ballVY;
    if (ballX < BALL_R + 1) {
      ballX = BALL_R + 1;
      ballVX = abs(ballVX);
    } else if (ballX > SCR_W - BALL_R - 1) {
      ballX = SCR_W - BALL_R - 1;
      ballVX = -abs(ballVX);
    }
    if (ballY < BR_TOP + BALL_R) {
      ballY = BR_TOP + BALL_R;
      ballVY = abs(ballVY);
    }
    // paddle hit
    if (ballVY > 0 && ballY + BALL_R >= PADDLE_Y &&
        ballY + BALL_R <= PADDLE_Y + PADDLE_H + 4 &&
        abs((int)(ballX - paddleX)) <= PADDLE_W / 2 + BALL_R) {
      ballVY = -abs(ballVY);
      ballVX = (ballX - paddleX) / (PADDLE_W / 2) * 2.2;
      ballY = PADDLE_Y - BALL_R - 1;
    }
    // brick hit
    int c = ((int)ballX - BR_X0) / (BR_W + BR_GAP);
    int r = ((int)ballY - BR_Y0) / (BR_H + BR_GAP);
    if (c >= 0 && c < BR_COLS && r >= 0 && r < BR_ROWS &&
        brBricks[r * BR_COLS + c]) {
      brBricks[r * BR_COLS + c] = 0;
      tft.fillRect(BR_X0 + c * (BR_W + BR_GAP), BR_Y0 + r * (BR_H + BR_GAP),
                   BR_W, BR_H, BLACK);
      ballVY = -ballVY;
      brScore += 10;
      brInfo();
      if (ballSpeed < 4.5) {
        ballSpeed += 0.05;
        ballVX *= 1.01;
        ballVY *= 1.01;
      }
      if (--brLeft == 0) {
        brLevel++;
        ballSpeed += 0.4;
        brOverlay(F("LEVEL CLEAR!"), F("get ready..."), GREEN);
        delay(1500);
        tft.fillRect(0, BR_TOP, SCR_W, SCR_H - BR_TOP, BLACK);
        brBuildBricks();
        brResetBall();
        lastStep = millis();
        continue;
      }
    }
    // missed
    if (ballY > SCR_H - 2) {
      brLives--;
      brInfo();
      if (brLives <= 0) {
        brOverlay(F("GAME OVER"), F("tap: retry  menu: quit"), RED);
        if (!brWaitTap()) return;
        brScore = 0;
        brLives = 3;
        brLevel = 1;
        ballSpeed = 2.2;
        tft.fillRect(0, BR_TOP, SCR_W, SCR_H - BR_TOP, BLACK);
        brInfo();
        brBuildBricks();
        brResetBall();
        lastStep = millis();
        continue;
      }
      brResetBall();
      delay(600);
      lastStep = millis();
      continue;
    }
    tft.fillCircle((int)ballX, (int)ballY, BALL_R, WHITE);
  }
}

// ================= game 3: snake =================
#define SN_CELL 10
#define SN_COLS 22
#define SN_ROWS 24
#define SN_X0 10
#define SN_Y0 64
#define SN_MAX 120

uint8_t snX[SN_MAX], snY[SN_MAX];
int snLen, snDir, snScore, snStepMs;
int snAppleX, snAppleY;

void snCellXY(int c, int r, int &x, int &y) {
  x = SN_X0 + c * SN_CELL;
  y = SN_Y0 + r * SN_CELL;
}

void snDrawSeg(int c, int r, uint16_t color) {
  int x, y;
  snCellXY(c, r, x, y);
  tft.fillRect(x + 1, y + 1, SN_CELL - 2, SN_CELL - 2, color);
}

void snEraseCell(int c, int r) {
  int x, y;
  snCellXY(c, r, x, y);
  tft.fillRect(x, y, SN_CELL, SN_CELL, BLACK);
}

void snDrawApple() {
  int x, y;
  snCellXY(snAppleX, snAppleY, x, y);
  tft.fillCircle(x + SN_CELL / 2, y + SN_CELL / 2, 4, RED);
  tft.fillRect(x + SN_CELL / 2, y + 1, 2, 2, GREEN);
}

void snScoreLine() {
  tft.fillRect(0, 32, SCR_W, 16, BLACK);
  tft.setTextSize(1);
  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(6, 36);
  tft.print(F("SCORE "));
  tft.print(snScore);
  tft.setCursor(150, 36);
  tft.print(F("LEN "));
  tft.print(snLen);
}

bool snOnSnake(int c, int r) {
  for (int i = 0; i < snLen; i++)
    if (snX[i] == c && snY[i] == r) return true;
  return false;
}

void snNewApple() {
  for (int tries = 0; tries < 200; tries++) {
    int c = random(SN_COLS), r = random(SN_ROWS);
    if (!snOnSnake(c, r)) {
      snAppleX = c;
      snAppleY = r;
      snDrawApple();
      return;
    }
  }
  // field nearly full: scan for any free cell
  for (int r = 0; r < SN_ROWS; r++)
    for (int c = 0; c < SN_COLS; c++)
      if (!snOnSnake(c, r)) {
        snAppleX = c;
        snAppleY = r;
        snDrawApple();
        return;
      }
}

void snInit() {
  drawHeader(F("SNAKE"));
  snScoreLine();
  tft.setTextSize(1);
  tft.setTextColor(DARKGREY, BLACK);
  tft.setCursor(72, 50);
  tft.print(F("Swipe to steer"));
  tft.drawRect(SN_X0 - 2, SN_Y0 - 2, SN_COLS * SN_CELL + 4,
               SN_ROWS * SN_CELL + 4, DARKGREY);
  snLen = 4;
  snDir = 1;  // moving right
  snScore = 0;
  snStepMs = 150;
  int hc = SN_COLS / 2, hr = SN_ROWS / 2;
  for (int i = 0; i < snLen; i++) {
    snX[i] = hc - i;
    snY[i] = hr;
    snDrawSeg(snX[i], snY[i], GREEN);
  }
  snDrawSeg(snX[0], snY[0], YELLOW);
  snNewApple();
  snScore = 0;
  snScoreLine();
}

void playSnake() {
  snInit();
  bool tracking = false;
  int startX = 0, startY = 0;
  unsigned long lastStep = millis();

  while (true) {
    int sx, sy;
    if (readTouch(sx, sy)) {
      if (inMenu(sx, sy)) return;
      if (!tracking) {
        tracking = true;
        startX = sx;
        startY = sy;
      } else {
        int dx = sx - startX, dy = sy - startY;
        if (abs(dx) > 16 || abs(dy) > 16) {
          if (abs(dx) > abs(dy))
            snDir = (dx > 0) ? 1 : 3;
          else
            snDir = (dy > 0) ? 2 : 0;
          startX = sx;
          startY = sy;
        }
      }
    } else {
      tracking = false;
    }
    if (millis() - lastStep < (unsigned long)snStepMs) continue;
    lastStep = millis();

    int nc = snX[0], nr = snY[0];
    if (snDir == 0) nr--;
    else if (snDir == 1) nc++;
    else if (snDir == 2) nr++;
    else nc--;

    bool dead = (nc < 0 || nc >= SN_COLS || nr < 0 || nr >= SN_ROWS);
    // moving into own tail cell is OK when tail moves away this step
    bool eats = (!dead && nc == snAppleX && nr == snAppleY);
    if (!dead) {
      int check = eats ? snLen : snLen - 1;
      for (int i = 0; i < check; i++)
        if (snX[i] == nc && snY[i] == nr) {
          dead = true;
          break;
        }
      // forbid 180-degree turn into neck
      if (snLen > 1 && nc == snX[1] && nr == snY[1]) dead = true;
    }
    if (dead) {
      tft.fillRoundRect(30, 150, 180, 80, 8, NAVY);
      tft.drawRoundRect(30, 150, 180, 80, 8, WHITE);
      centerText(F("GAME OVER"), 160, 2, RED, NAVY);
      tft.setTextSize(2);
      tft.setTextColor(WHITE, NAVY);
      tft.setCursor(62, 186);
      tft.print(F("Score "));
      tft.print(snScore);
      centerText(F("tap: retry  menu: quit"), 210, 1, WHITE, NAVY);
      waitRelease();
      bool retry = false;
      while (true) {
        if (readTouch(sx, sy)) {
          if (inMenu(sx, sy)) return;
          retry = true;
          break;
        }
        delay(30);
      }
      if (retry) {
        waitRelease();
        snInit();
        tracking = false;
        lastStep = millis();
        continue;
      }
      return;
    }

    if (eats) {
      if (snLen < SN_MAX) {
        for (int i = snLen; i > 0; i--) {
          snX[i] = snX[i - 1];
          snY[i] = snY[i - 1];
        }
        snLen++;
      } else {
        for (int i = snLen - 1; i > 0; i--) {
          snX[i] = snX[i - 1];
          snY[i] = snY[i - 1];
        }
        snEraseCell(snX[snLen - 1], snY[snLen - 1]);
      }
      snX[0] = nc;
      snY[0] = nr;
      snDrawSeg(snX[1], snY[1], GREEN);
      snDrawSeg(nc, nr, YELLOW);
      snScore += 10;
      if (snStepMs > 70) snStepMs -= 3;
      snScoreLine();
      snNewApple();
    } else {
      snEraseCell(snX[snLen - 1], snY[snLen - 1]);
      for (int i = snLen - 1; i > 0; i--) {
        snX[i] = snX[i - 1];
        snY[i] = snY[i - 1];
      }
      snX[0] = nc;
      snY[0] = nr;
      snDrawSeg(snX[1], snY[1], GREEN);
      snDrawSeg(nc, nr, YELLOW);
    }
  }
}

// ================= setup / main loop =================
void setup() {
  Serial.begin(9600);
  gID = tft.readID();
  if (gID == 0xD3D3) gID = 0x9341;  // write-only shield
  Serial.print(F("TFT ID: 0x"));
  Serial.println(gID, HEX);
  tft.begin(gID);
  tft.setRotation(0);  // portrait 240x320
  randomSeed(analogRead(A5));
  loadCal();
}

void loop() {
  showMenu();
  waitRelease();
  while (true) {
    int sx, sy;
    if (!readTouch(sx, sy)) {
      delay(30);
      continue;
    }
    if (sx >= BTN_X && sx <= BTN_X + BTN_W) {
      for (int i = 0; i < 3; i++) {
        if (sy >= btnY[i] && sy <= btnY[i] + BTN_H) {
          waitRelease();
          if (i == 0) playTicTacToe();
          else if (i == 1) playBreakout();
          else playSnake();
          return;  // redraw menu
        }
      }
    }
    if (sx >= 70 && sx <= 170 && sy >= 290 && sy <= 314) {
      waitRelease();
      runCalibration();
      return;
    }
    waitRelease();
  }
}
