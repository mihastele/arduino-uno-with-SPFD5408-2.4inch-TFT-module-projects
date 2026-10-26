// GHOST_SHELL — green hacker animation for Arduino UNO + 2.4" MCUFRIEND TFT
//
// - Rotating 3D wireframe cube (outer + inner, linked vertices) with
//   non-deterministic rotation speed, size and drift
// - Radar sweep, matrix rain, hex speckles, glitch slices
// - Live fake terminal typing random linux commands with random speed
// - No touch needed. Everything is random: seeded from floating analog
//   pin noise + micros(), re-stirred periodically.
//
// Wiring: 2.4" TFT LCD shield plugged directly onto the UNO.
// Libs: MCUFRIEND_kbv, Adafruit_GFX (install via Library Manager)

#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>
#include <avr/pgmspace.h>

MCUFRIEND_kbv tft;

// ---------------- colors (green on black) ----------------
#define COL_BG     0x0000
#define COL_BRIGHT 0x07E0  // 0,255,0
#define COL_OUT    0x06E0  // 0,220,0
#define COL_MID    0x0540  // 0,170,0
#define COL_DIM    0x02C0  // 0,90,0
#define COL_FAINT  0x0160  // 0,45,0
#define COL_CMD    0x97F6  // pale green for typed commands

// ---------------- layout (320x240 landscape) ----------------
#define ZONE_Y0 22
#define ZONE_Y1 149
#define CUBE_CX 160
#define CUBE_CY 85

#define TERM_X 4
#define TERM_Y 156
#define TERM_W 312
#define TERM_H 80
#define TERM_ROWS 10
#define TERM_COLS 52

// ---------------- cube geometry ----------------
const int8_t VERT[8][3] = {
  {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
  {-1,-1, 1}, {1,-1, 1}, {1,1, 1}, {-1,1, 1}
};
const uint8_t EDGE[12][2] = {
  {0,1},{1,2},{2,3},{3,0},
  {4,5},{5,6},{6,7},{7,4},
  {0,4},{1,5},{2,6},{3,7}
};

// cube motion state (eased toward random targets)
float angX, angY, angZ;
float velX, velY, velZ, tgtVX, tgtVY, tgtVZ;
float cubeSize, tgtSize;
float cubeCX, cubeCY, tgtCX, tgtCY;
int16_t retargetIn;          // frames until new random targets
float sweepA;                // radar sweep angle

// matrix rain drops
#define DROPS 20
struct Drop { int16_t x, y; uint8_t sp, len; };
Drop drops[DROPS];

// hex speckle (one visible at a time, erased before next)
int16_t speckX = -1, speckY = -1;

// glitch slices
int16_t glitchY[3], glitchX[3], glitchW[3];
uint32_t glitchEraseAt = 0;
uint32_t nextGlitch = 0;
uint32_t nextSpeck = 0;

// ---------------- terminal buffer ----------------
char     termBuf[TERM_ROWS][TERM_COLS + 1];
uint16_t termCol[TERM_ROWS];
uint8_t  termPrompt[TERM_ROWS];   // prompt-length prefix drawn bright
uint8_t  termUsed = 0;

// cursor
int16_t curX = TERM_X, curY = TERM_Y;
bool    curVisible = false;
uint32_t lastBlink = 0;

// ---------------- command pool (PROGMEM) ----------------
const char P0[] PROGMEM = "root@ghost:~# ";
const char P1[] PROGMEM = "ghost@darknet:~$ ";
const char P2[] PROGMEM = "[0x3f]# ";
const char * const PROMPTS[] PROGMEM = { P0, P1, P2 };
#define N_PROMPTS 3

const char C00[] PROGMEM = "nmap -sS -p- 10.0.3.15";
const char C01[] PROGMEM = "ssh ghost@10.0.3.15";
const char C02[] PROGMEM = "tail -f /var/log/auth.log";
const char C03[] PROGMEM = "hydra -l admin ssh://10.0.3.15";
const char C04[] PROGMEM = "tcpdump -i eth0 port 443";
const char C05[] PROGMEM = "./ghost --inject -p 4444";
const char C06[] PROGMEM = "john --wordlist=rockyou shadow";
const char C07[] PROGMEM = "msfconsole -q -x run";
const char C08[] PROGMEM = "cat /etc/shadow";
const char C09[] PROGMEM = "ping -c3 darknet.onion";
const char C10[] PROGMEM = "ls -la /root/.hidden";
const char C11[] PROGMEM = "python3 -m http.server 8080";
const char C12[] PROGMEM = "aircrack-ng capture.cap";
const char C13[] PROGMEM = "whoami && id";
const char C14[] PROGMEM = "rm -rf / --no-preserve-root";
const char C15[] PROGMEM = "dmesg | grep -i tty";
const char * const CMDS[] PROGMEM = {
  C00,C01,C02,C03,C04,C05,C06,C07,
  C08,C09,C10,C11,C12,C13,C14,C15
};
#define N_CMDS 16

const char O00[] PROGMEM = "22/tcp open ssh\n80/tcp open http\n443/tcp open https\n[*] done in 3.17s";
const char O01[] PROGMEM = "authenticating...\nACCESS GRANTED :: 0.42s";
const char O02[] PROGMEM = "Failed pwd root from 185.22.1.9\nAccepted key ghost from 10.0.3.1";
const char O03[] PROGMEM = "[22][ssh] login admin pass *******\n1 valid password found";
const char O04[] PROGMEM = "10.0.3.15.443 > us: Flags [P.]\n10.0.3.15.443 > us: Flags [P.]\n5 packets captured";
const char O05[] PROGMEM = "[+] payload injected\n[+] shell listening on 4444";
const char O06[] PROGMEM = "cracked: letmein (root)\ncracked: ******* (admin)";
const char O07[] PROGMEM = "[*] handler started\n[*] sending stage...";
const char O08[] PROGMEM = "root:$6$Xk92...:19800::::::\nghost:$6$Qw44...:19800::::::";
const char O09[] PROGMEM = "64B from darknet: time=1.37ms\n64B from darknet: time=0.98ms\n64B from darknet: time=2.11ms";
const char O10[] PROGMEM = "-rwx------ ghost_v2.sh\n-rw-r--r-- keys.db\ndrwxr-xr-x loot/";
const char O11[] PROGMEM = "serving at port 8080...\n[+] GET /payload.sh 200";
const char O12[] PROGMEM = "[00:00:42] tested 81234 keys\nKEY FOUND! [ hunter2 ]";
const char O13[] PROGMEM = "root\nuid=0(root) gid=0(root)";
const char O14[] PROGMEM = "removing /dev/morals...\nremoving /var/traces...\n[!] no survivors";
const char O15[] PROGMEM = "[2.114] Arduino UNO detected\n[2.118] ch341 attached ttyUSB0";
const char * const OUTS[] PROGMEM = {
  O00,O01,O02,O03,O04,O05,O06,O07,
  O08,O09,O10,O11,O12,O13,O14,O15
};

// ---------------- terminal state machine ----------------
enum TState : uint8_t { ST_PAUSE, ST_TYPING, ST_OUTPUT };
TState tState = ST_PAUSE;
uint32_t stateUntil = 0;
uint32_t nextCharAt = 0;
uint8_t  cmdIdx = 0, lastCmd = 255, promptIdx = 0;
uint16_t charPos = 0;     // position inside prompt/cmd/output
uint8_t  typeStage = 0;   // 0 = prompt, 1 = command
uint16_t outLen = 0;

// header stats
uint32_t startSec = 0;
uint32_t lastSecShown = 0xFFFFFFFF;
uint32_t lastStats = 0;
int8_t cpuPct = 47, memPct = 63;
bool recOn = false;
uint32_t lastRec = 0;

// ---------------- helpers ----------------
float randFloat(float a, float b) {
  return a + (b - a) * (random(1000) / 1000.0f);
}

// stir fresh hardware noise into the PRNG (non-deterministic loop)
void stirEntropy() {
  uint32_t e = micros();
  e ^= (uint32_t)analogRead(A5) << 9;
  e ^= (uint32_t)analogRead(A5);
  randomSeed(random(0x7FFFFFFF) ^ e);
}

const char *readProgStr(const char * const *arr, uint8_t i) {
  return (const char *)pgm_read_ptr(&arr[i]);
}

// ---------------- terminal drawing ----------------
void moveCursorToEnd() {
  uint8_t r = termUsed ? termUsed - 1 : 0;
  curX = TERM_X + (int16_t)strlen(termBuf[r]) * 6;
  curY = TERM_Y + r * 8;
  if (curX > TERM_X + TERM_W - 6) curX = TERM_X + TERM_W - 6;
}

void termRedraw() {
  tft.fillRect(TERM_X - 2, TERM_Y - 2, TERM_W + 4, TERM_H + 4, COL_BG);
  tft.setTextSize(1);
  for (uint8_t i = 0; i < termUsed; i++) {
    tft.setCursor(TERM_X, TERM_Y + i * 8);
    tft.setTextColor(COL_BRIGHT);
    for (uint8_t k = 0; k < termPrompt[i] && termBuf[i][k]; k++)
      tft.print(termBuf[i][k]);
    tft.setTextColor(termCol[i]);
    tft.print(&termBuf[i][termPrompt[i]]);
  }
  moveCursorToEnd();
}

void termBeginLine(uint16_t col, uint8_t promptLen) {
  if (termUsed >= TERM_ROWS) {
    for (uint8_t i = 0; i < TERM_ROWS - 1; i++) {
      strcpy(termBuf[i], termBuf[i + 1]);
      termCol[i] = termCol[i + 1];
      termPrompt[i] = termPrompt[i + 1];
    }
  } else {
    termUsed++;
  }
  uint8_t r = termUsed - 1;
  termBuf[r][0] = '\0';
  termCol[r] = col;
  termPrompt[r] = promptLen;
}

void termAddChar(char c) {
  if (!termUsed) return;
  uint8_t r = termUsed - 1;
  uint8_t n = strlen(termBuf[r]);
  if (n < TERM_COLS) {
    termBuf[r][n] = c;
    termBuf[r][n + 1] = '\0';
  }
}

void termAddStr(const char *s) {
  while (*s) termAddChar(*s++);
}

void termAddStr(const __FlashStringHelper *fs) {
  const char *p = (const char *)fs;
  char c;
  while ((c = pgm_read_byte(p++))) termAddChar(c);
}

void termCommitLine() {
  termRedraw();
}

// type one char live (no full redraw)
void typeChar(char c, uint16_t col) {
  termAddChar(c);
  tft.setCursor(curX, curY);
  tft.setTextColor(col);
  tft.print(c);
  curX += 6;
}

void setCursorVis(bool on) {
  if (on == curVisible) return;
  curVisible = on;
  tft.fillRect(curX, curY, 5, 7, on ? COL_BRIGHT : COL_BG);
}

// ---------------- cube rendering ----------------
// projects current angles; call with all BLACK to erase
void renderCube(uint16_t colEdge, uint16_t colInner,
                uint16_t colLink, uint16_t colDot) {
  int16_t px[8][2], qx[8][2];

  float cx = cos(angX), sx = sin(angX);
  float cy = cos(angY), sy = sin(angY);
  float cz = cos(angZ), sz = sin(angZ);
  // inner cube spins on its own (opposite-ish) angles
  float bx = -angX * 1.7f, by = angY * 1.2f + 0.4f, bz = -angZ * 1.5f;
  float cbx = cos(bx), sbx = sin(bx);
  float cby = cos(by), sby = sin(by);
  float cbz = cos(bz), sbz = sin(bz);

  for (uint8_t i = 0; i < 8; i++) {
    float x = VERT[i][0], y = VERT[i][1], z = VERT[i][2];
    // outer: rotate X -> Y -> Z
    float y1 = y * cx - z * sx, z1 = y * sx + z * cx;
    float x2 = x * cy + z1 * sy, z2 = -x * sy + z1 * cy;
    float x3 = x2 * cz - y1 * sz, y3 = x2 * sz + y1 * cz;
    float s = cubeSize / (3.0f - z2);
    px[i][0] = (int16_t)(cubeCX + x3 * s);
    px[i][1] = (int16_t)(cubeCY + y3 * s);
    // inner cube (scaled 0.45)
    float ix = x * 0.45f, iy = y * 0.45f, iz = z * 0.45f;
    float jy1 = iy * cbx - iz * sbx, jz1 = iy * sbx + iz * cbx;
    float jx2 = ix * cby + jz1 * sby, jz2 = -ix * sby + jz1 * cby;
    float jx3 = jx2 * cbz - jy1 * sbz, jy3 = jx2 * sbz + jy1 * cbz;
    float js = cubeSize / (3.0f - jz2);
    qx[i][0] = (int16_t)(cubeCX + jx3 * js);
    qx[i][1] = (int16_t)(cubeCY + jy3 * js);
  }

  for (uint8_t e = 0; e < 12; e++) {
    uint8_t a = EDGE[e][0], b = EDGE[e][1];
    tft.drawLine(px[a][0], px[a][1], px[b][0], px[b][1], colEdge);
    tft.drawLine(qx[a][0], qx[a][1], qx[b][0], qx[b][1], colInner);
  }
  for (uint8_t i = 0; i < 8; i++) {
    tft.drawLine(px[i][0], px[i][1], qx[i][0], qx[i][1], colLink);
    tft.fillCircle(px[i][0], px[i][1], 2, colDot);
    tft.fillCircle(qx[i][0], qx[i][1], 1, colDot);
  }
}

void pickMotionTargets() {
  tgtVX = randFloat(-0.085f, 0.085f);
  tgtVY = randFloat(-0.085f, 0.085f);
  tgtVZ = randFloat(-0.06f, 0.06f);
  if (fabs(tgtVX) < 0.015f) tgtVX += (tgtVX >= 0 ? 0.025f : -0.025f);
  if (fabs(tgtVY) < 0.015f) tgtVY += (tgtVY >= 0 ? 0.025f : -0.025f);
  // occasional wild spin burst
  if (random(100) < 10) {
    tgtVX *= 3.0f; tgtVY *= 3.0f;
  }
  tgtVX = constrain(tgtVX, -0.22f, 0.22f);
  tgtVY = constrain(tgtVY, -0.22f, 0.22f);
  tgtSize = random(46, 72);
  tgtCX = random(118, 202);
  tgtCY = random(62, 108);
  retargetIn = random(120, 380);
}

void updateMotion() {
  velX += (tgtVX - velX) * 0.02f;
  velY += (tgtVY - velY) * 0.02f;
  velZ += (tgtVZ - velZ) * 0.02f;
  cubeSize += (tgtSize - cubeSize) * 0.02f;
  cubeCX += (tgtCX - cubeCX) * 0.015f;
  cubeCY += (tgtCY - cubeCY) * 0.015f;
  angX += velX; angY += velY; angZ += velZ;
  if (--retargetIn <= 0) {
    pickMotionTargets();
    if (random(100) < 30) stirEntropy();  // stay non-deterministic
  }
}

void drawSweep(uint16_t col) {
  int16_t x1 = CUBE_CX + (int16_t)(cos(sweepA) * 60);
  int16_t y1 = CUBE_CY + (int16_t)(sin(sweepA) * 60);
  tft.drawLine(CUBE_CX, CUBE_CY, x1, y1, col);
}

void updateDrops() {
  for (uint8_t i = 0; i < DROPS; i++) {
    // erase old
    tft.drawLine(drops[i].x, drops[i].y - drops[i].len, drops[i].x, drops[i].y, COL_BG);
    drops[i].y += drops[i].sp;
    if (drops[i].y - drops[i].len > ZONE_Y1) {
      drops[i].x = random(6, 314);
      drops[i].y = ZONE_Y0 + random(0, 20);
      drops[i].sp = random(1, 4);
      drops[i].len = random(4, 12);
    }
    // draw new
    tft.drawLine(drops[i].x, drops[i].y - drops[i].len, drops[i].x, drops[i].y - 1, COL_DIM);
    tft.drawPixel(drops[i].x, drops[i].y, COL_MID);
  }
}

void updateSpeckle(uint32_t now) {
  if (now < nextSpeck) return;
  nextSpeck = now + random(250, 900);
  tft.setTextSize(1);
  // erase previous
  if (speckX >= 0) {
    tft.setCursor(speckX, speckY);
    tft.setTextColor(COL_BG);
    tft.print(F("0000"));
  }
  speckX = random(8, 290);
  speckY = random(ZONE_Y0 + 2, ZONE_Y1 - 10);
  tft.setCursor(speckX, speckY);
  tft.setTextColor(COL_DIM);
  char hex[5];
  sprintf(hex, "%04X", (unsigned)random(0x10000));
  tft.print(hex);
}

void updateGlitch(uint32_t now) {
  if (glitchEraseAt && now >= glitchEraseAt) {
    for (uint8_t i = 0; i < 3; i++)
      tft.fillRect(glitchX[i], glitchY[i], glitchW[i], 2, COL_BG);
    glitchEraseAt = 0;
  }
  if (now >= nextGlitch) {
    nextGlitch = now + random(2500, 9000);
    for (uint8_t i = 0; i < 3; i++) {
      glitchY[i] = random(ZONE_Y0 + 2, ZONE_Y1 - 3);
      glitchX[i] = random(0, 240);
      glitchW[i] = random(30, 120);
      tft.fillRect(glitchX[i], glitchY[i], glitchW[i], 2, COL_MID);
    }
    glitchEraseAt = now + 70;
  }
}

// ---------------- header ----------------
void drawHeaderFrame() {
  tft.fillRect(0, 0, 320, 20, COL_BG);
  tft.drawLine(0, 20, 319, 20, COL_DIM);
  tft.setTextSize(1);
  tft.setCursor(4, 3);
  tft.setTextColor(COL_BRIGHT);
  tft.print(F("GHOST_SHELL v2.4"));
  tft.setCursor(286, 11);
  tft.setTextColor(COL_MID);
  tft.print(F("REC"));
}

void updateHeader(uint32_t now) {
  // clock each second
  uint32_t sec = startSec + now / 1000;
  if (sec != lastSecShown) {
    lastSecShown = sec;
    char t[9];
    sprintf(t, "%02lu:%02lu:%02lu",
            (sec / 3600) % 100, (sec / 60) % 60, sec % 60);
    tft.fillRect(262, 2, 54, 8, COL_BG);
    tft.setCursor(262, 3);
    tft.setTextColor(COL_MID);
    tft.print(t);
  }
  // stats ~1.4 Hz random walk
  if (now - lastStats > 700) {
    lastStats = now;
    cpuPct = constrain(cpuPct + random(-9, 10), 6, 98);
    memPct = constrain(memPct + random(-6, 7), 28, 94);
    char s[30];
    sprintf(s, "CPU %2d%% MEM %2d%% %4dK/s",
            cpuPct, memPct, (int)random(120, 2400));
    tft.fillRect(4, 10, 200, 9, COL_BG);
    tft.setCursor(4, 11);
    tft.setTextColor(COL_DIM);
    tft.print(s);
  }
  // blinking REC dot
  if (now - lastRec > 500) {
    lastRec = now;
    recOn = !recOn;
    tft.fillCircle(312, 14, 3, recOn ? COL_BRIGHT : COL_FAINT);
  }
}

// ---------------- animation zone frame ----------------
void drawZoneFrame() {
  tft.drawLine(0, ZONE_Y1 + 1, 319, ZONE_Y1 + 1, COL_DIM);
  tft.drawCircle(CUBE_CX, CUBE_CY, 60, COL_FAINT);
  tft.drawCircle(CUBE_CX, CUBE_CY, 44, COL_FAINT);
  // crosshair ticks
  tft.drawLine(CUBE_CX - 66, CUBE_CY, CUBE_CX - 60, CUBE_CY, COL_DIM);
  tft.drawLine(CUBE_CX + 60, CUBE_CY, CUBE_CX + 66, CUBE_CY, COL_DIM);
  tft.drawLine(CUBE_CX, CUBE_CY - 66, CUBE_CX, CUBE_CY - 60, COL_DIM);
  tft.drawLine(CUBE_CX, CUBE_CY + 60, CUBE_CX, CUBE_CY + 66, COL_DIM);
  // corner brackets
  uint16_t c = COL_DIM;
  tft.drawLine(2, 26, 12, 26, c); tft.drawLine(2, 26, 2, 36, c);
  tft.drawLine(307, 26, 317, 26, c); tft.drawLine(317, 26, 317, 36, c);
  tft.drawLine(2, 145, 12, 145, c); tft.drawLine(2, 135, 2, 145, c);
  tft.drawLine(307, 145, 317, 145, c); tft.drawLine(317, 135, 317, 145, c);
}

// ---------------- terminal state machine ----------------
void updateTerm(uint32_t now) {
  switch (tState) {
    case ST_PAUSE: {
      if (now < stateUntil) break;
      // pick a new random command (avoid immediate repeat)
      do { cmdIdx = random(N_CMDS); } while (cmdIdx == lastCmd);
      lastCmd = cmdIdx;
      promptIdx = random(N_PROMPTS);
      termBeginLine(COL_CMD, 0);
      moveCursorToEnd();
      charPos = 0;
      typeStage = 0;
      tState = ST_TYPING;
      nextCharAt = now + random(150, 500);
      break;
    }
    case ST_TYPING: {
      if (now < nextCharAt) break;
      if (typeStage == 0) {
        const char *p = readProgStr(PROMPTS, promptIdx);
        char c = pgm_read_byte(p + charPos);
        if (c == '\0') {
          typeStage = 1;
          charPos = 0;
          termPrompt[termUsed - 1] = strlen(termBuf[termUsed - 1]);
          nextCharAt = now + random(120, 350);
        } else {
          typeChar(c, COL_BRIGHT);
          charPos++;
          nextCharAt = now + random(20, 60);
        }
      } else {
        const char *p = readProgStr(CMDS, cmdIdx);
        char c = pgm_read_byte(p + charPos);
        if (c == '\0') {
          const char *o = readProgStr(OUTS, cmdIdx);
          outLen = strlen_P(o);
          charPos = 0;
          tState = ST_OUTPUT;
          stateUntil = now + random(250, 700);
        } else {
          typeChar(c, COL_CMD);
          charPos++;
          uint8_t r = random(100);
          uint16_t d = (r < 8) ? random(180, 520)   // thinking pause
                     : (r < 22) ? random(4, 14)     // burst
                     : random(18, 85);              // normal typing
          nextCharAt = now + d;
        }
      }
      break;
    }
    case ST_OUTPUT: {
      if (now < stateUntil) break;
      // emit one output line per step
      const char *o = readProgStr(OUTS, cmdIdx);
      termBeginLine(COL_OUT, 0);
      while (charPos < outLen) {
        char c = pgm_read_byte(o + charPos++);
        if (c == '\n') break;
        termAddChar(c);
      }
      termCommitLine();
      if (charPos >= outLen) {
        // random bonus line for extra non-determinism
        uint8_t r = random(100);
        if (r < 25) {
          char tmp[40];
          sprintf(tmp, "[*] session 0x%04X :: %u pkts",
                  (unsigned)random(0x1000, 0xFFFF),
                  (unsigned)random(128, 9999));
          termBeginLine(COL_DIM, 0);
          termAddStr(tmp);
          termCommitLine();
        } else if (r < 35) {
          termBeginLine(COL_DIM, 0);
          termAddStr(F("[!] IDS evaded :: rotating IP..."));
          termCommitLine();
        }
        tState = ST_PAUSE;
        stateUntil = now + random(500, 1800);
        if (random(100) < 20) stirEntropy();
      } else {
        stateUntil = now + random(90, 380);
      }
      break;
    }
  }
}

// ---------------- splash ----------------
void splash() {
  tft.fillScreen(COL_BG);
  tft.setTextSize(3);
  tft.setCursor(40, 60);
  tft.setTextColor(COL_BRIGHT);
  tft.print(F("GHOST_"));
  tft.setTextColor(COL_MID);
  tft.print(F("SHELL"));
  tft.setTextSize(1);
  tft.setCursor(40, 100);
  tft.setTextColor(COL_MID);
  tft.print(F(">> bypassing firewall..."));
  tft.drawRect(58, 130, 204, 14, COL_DIM);
  int16_t p = 0;
  while (p < 100) {
    p = min(100, p + random(4, 16));
    tft.fillRect(60, 132, p * 2, 10, COL_MID);
    tft.fillRect(60 + p * 2, 132, 200 - p * 2, 10, COL_BG);
    tft.setCursor(60, 148);
    tft.setTextColor(COL_DIM);
    tft.print(F("exploit: "));
    tft.print(p);
    tft.print(F("%  "));
    delay(random(60, 170));
  }
  delay(350);
}

// ---------------- arduino entry ----------------
void setup() {
  // entropy: sample floating A5 + timing jitter
  uint32_t e = 0;
  for (uint8_t i = 0; i < 10; i++) {
    e = (e << 3) ^ analogRead(A5) ^ micros();
    delay(7);
  }
  randomSeed(e);

  uint16_t id = tft.readID();
  tft.begin(id);
  tft.setRotation(1);  // 320x240 landscape
  tft.setTextWrap(false);

  splash();

  tft.fillScreen(COL_BG);
  drawHeaderFrame();
  drawZoneFrame();

  // init cube motion
  angX = randFloat(0, 6.28f); angY = randFloat(0, 6.28f); angZ = randFloat(0, 6.28f);
  cubeSize = 56; cubeCX = CUBE_CX; cubeCY = CUBE_CY;
  velX = velY = velZ = 0;
  pickMotionTargets();
  sweepA = randFloat(0, 6.28f);

  // init rain
  for (uint8_t i = 0; i < DROPS; i++) {
    drops[i].x = random(6, 314);
    drops[i].y = random(ZONE_Y0, ZONE_Y1);
    drops[i].sp = random(1, 4);
    drops[i].len = random(4, 12);
  }
  nextGlitch = millis() + random(2000, 5000);
  nextSpeck = millis() + 500;

  startSec = random(0, 80000);

  // boot lines
  termBeginLine(COL_OUT, 0);
  termAddStr(F("[+] secure channel established"));
  termCommitLine();
  delay(250);
  termBeginLine(COL_OUT, 0);
  termAddStr(F("[+] firewall bypassed :: ghost mode"));
  termCommitLine();
  delay(250);

  tState = ST_PAUSE;
  stateUntil = millis() + 600;
}

void loop() {
  uint32_t now = millis();

  // erase old frame
  drawSweep(COL_BG);
  renderCube(COL_BG, COL_BG, COL_BG, COL_BG);

  // advance + draw new frame
  updateMotion();
  sweepA += 0.045f + random(20) / 10000.0f;
  renderCube(COL_BRIGHT, COL_MID, COL_DIM, COL_BRIGHT);
  drawSweep(COL_DIM);

  updateDrops();
  updateSpeckle(now);
  updateGlitch(now);
  updateHeader(now);
  updateTerm(now);

  // cursor blink
  if (now - lastBlink > 280) {
    lastBlink = now;
    setCursorVis(!curVisible);
  }

  delay(15);
}
