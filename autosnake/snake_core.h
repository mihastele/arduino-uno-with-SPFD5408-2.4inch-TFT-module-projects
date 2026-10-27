// autosnake core: grid, Hamiltonian cycle, and self-playing AI.
//
// Pure C++ with no Arduino dependencies, so the exact same logic can be
// compiled and simulated on a desktop for verification.
//
// Why the snake always wins (all premises are asserted by the simulator):
//   - The grid is covered by a fixed Hamiltonian cycle: one loop visiting
//     every cell exactly once per lap (see buildCycle()).
//   - The head advances exactly one cycle cell per tick, so the body is
//     always one contiguous cycle interval [head-len+1, head] (P1).
//   - While len < NCELLS, the next cycle cell is therefore always free:
//     it sits N-1 behind the head along the cycle, outside the body's
//     behind-range of 0..len-1 (P2). The snake can never collide.
//   - Food spawns on a free cell, which is always ahead of the head, so it
//     is always reached within one lap (P3). Repeat until the board is
//     full -> EV_WIN (P4).

#pragma once
#include <stdint.h>

namespace autosnake {

enum {
  GRID_W = 16,
  GRID_H = 12,  // must be even (Hamiltonian cycle construction below)
  NCELLS = GRID_W * GRID_H,  // 192, must stay < 256 (uint8_t indices)
  START_LEN = 4
};

enum {
  EV_MOVED = 0,
  EV_ATE = 1,
  EV_WIN = 2,  // board full: snake length == NCELLS
  EV_DEAD = 3  // defensive only: unreachable unless RAM is corrupted
};

struct StepResult {
  uint8_t event;
  uint8_t grew;
  uint8_t hx, hy;    // new head cell
  uint8_t ohx, ohy;  // old head cell (repaint as body)
  uint8_t tx, ty;    // old tail cell (erase unless grew)
};

struct Game {
  uint8_t cycPos[NCELLS];          // cycle index -> packed cell (y * W + x)
  uint8_t cycIdx[GRID_H][GRID_W];  // cell -> cycle index
  uint8_t body[NCELLS];            // circular queue of cycle indices, tail..head
  uint8_t occ[(NCELLS + 7) / 8];   // occupancy bitset, by cycle index
  uint8_t tailPtr, headPtr, len;
  uint8_t foodC;  // food, as cycle index
  uint8_t hasFood;

  static uint8_t cellX(uint8_t pos) { return (uint8_t)(pos % GRID_W); }
  static uint8_t cellY(uint8_t pos) { return (uint8_t)(pos / GRID_W); }
  static uint8_t pack(uint8_t x, uint8_t y) { return (uint8_t)(y * GRID_W + x); }
  static uint8_t cycDist(uint8_t from, uint8_t to) {
    return (uint8_t)(((uint16_t)to + NCELLS - from) % NCELLS);
  }

  uint8_t occGet(uint8_t c) const {
    return (uint8_t)((occ[c >> 3] >> (c & 7)) & 1);
  }
  void occSet(uint8_t c) { occ[c >> 3] |= (uint8_t)(1u << (c & 7)); }
  void occClr(uint8_t c) { occ[c >> 3] &= (uint8_t) ~(1u << (c & 7)); }

  void pushCyc(uint8_t x, uint8_t y, uint16_t &n) {
    cycPos[n] = pack(x, y);
    cycIdx[y][x] = (uint8_t)n;
    n++;
  }

  // Serpentine Hamiltonian cycle (needs even GRID_H):
  // row 0 left->right, rows 1..H-1 serpentine in columns 1..W-1,
  // then column 0 bottom->top closes the loop back to (0,0).
  void buildCycle() {
    uint16_t n = 0;
    for (uint8_t x = 0; x < GRID_W; x++) pushCyc(x, 0, n);
    for (uint8_t y = 1; y < GRID_H; y++) {
      if (y & 1) {
        for (int8_t x = GRID_W - 1; x >= 1; x--) pushCyc((uint8_t)x, y, n);
      } else {
        for (uint8_t x = 1; x < GRID_W; x++) pushCyc(x, y, n);
      }
    }
    for (int8_t y = GRID_H - 1; y >= 1; y--) pushCyc(0, (uint8_t)y, n);
  }

  void reset() {
    buildCycle();
    for (uint8_t i = 0; i < (uint8_t)sizeof(occ); i++) occ[i] = 0;
    for (uint8_t i = 0; i < START_LEN; i++) {
      body[i] = i;
      occSet(i);
    }
    tailPtr = 0;
    headPtr = START_LEN - 1;
    len = START_LEN;
    hasFood = 0;
    foodC = 0;
  }

  uint8_t freeCount() const { return (uint8_t)(NCELLS - len); }
  uint8_t headC() const { return body[headPtr]; }
  uint8_t tailC() const { return body[tailPtr]; }
  uint8_t headX() const { return cellX(cycPos[headC()]); }
  uint8_t headY() const { return cellY(cycPos[headC()]); }
  uint8_t foodX() const { return cellX(cycPos[foodC]); }
  uint8_t foodY() const { return cellY(cycPos[foodC]); }

  // k-th segment from tail (0) to head (len-1), as cycle index.
  uint8_t bodyAt(uint8_t k) const {
    return body[(uint8_t)((tailPtr + k) % NCELLS)];
  }

  // Spawn food on the pick-th free cell (requires pick < freeCount()).
  void spawnFood(uint16_t pick) {
    for (uint16_t c = 0; c < NCELLS; c++) {
      if (!occGet((uint8_t)c)) {
        if (pick == 0) {
          foodC = (uint8_t)c;
          hasFood = 1;
          return;
        }
        pick--;
      }
    }
  }

  // Advance exactly one cell along the Hamiltonian cycle. By (P2) the
  // target cell is always free while the board is not full.
  StepResult step() {
    StepResult r;
    r.grew = 0;
    uint8_t hc = headC();
    uint8_t tc = tailC();
    r.ohx = cellX(cycPos[hc]);
    r.ohy = cellY(cycPos[hc]);
    r.tx = cellX(cycPos[tc]);
    r.ty = cellY(cycPos[tc]);

    uint8_t nc = (uint8_t)((hc + 1) % NCELLS);
    if (occGet(nc)) {  // unreachable by (P2); guards against RAM corruption
      r.event = EV_DEAD;
      r.hx = r.ohx;
      r.hy = r.ohy;
      return r;
    }
    r.hx = cellX(cycPos[nc]);
    r.hy = cellY(cycPos[nc]);
    uint8_t grew = (nc == foodC) ? 1 : 0;
    r.grew = grew;
    if (!grew) {
      occClr(tc);
      tailPtr = (uint8_t)((tailPtr + 1) % NCELLS);
    }
    headPtr = (uint8_t)((headPtr + 1) % NCELLS);
    body[headPtr] = nc;
    occSet(nc);
    if (grew) {
      len++;
      hasFood = 0;
      r.event = (len == NCELLS) ? (uint8_t)EV_WIN : (uint8_t)EV_ATE;
    } else {
      r.event = EV_MOVED;
    }
    return r;
  }
};

}  // namespace autosnake
