#pragma once
// Move rules on the square grid, the move range of a selected unit, the path to the cursor,
// and how the range and the movement arrow are drawn.
#include "ArrowArt.h"

// Unit kinds, game order: 0-2 infantry, 3-9 vehicles, 10-12 planes, 13-15 ships.
// Move distance per kind, from the old game.
const uint8_t unitMove[16] PROGMEM = { 3, 2, 5,  6, 7, 4, 4, 3, 3, 4,  5, 7, 4,  5, 6, 7 };

// Terrain kinds (tools/mapconv.py): 0 Plains, 1 Forest, 2 Mountain, 3 Sea, 4 Shore, 5 Road, 6 Hill.
// Where each class of unit may go, one bit per terrain kind.
// ponytail: buildings are not checked. A ship may enter a building that stands on water.
static uint8_t terrainBits(uint8_t kind) {
  return kind < 3 ? 0b1100111 : kind < 10 ? 0b1100011 : kind < 13 ? 0b1111111 : 0b0011000;
}

// ---- Combat, numbers from the old game ----

// Per kind: attack (damage to a full health Soldier on plains), reach (attack range in grid steps),
// defense (taken off every hit).
const uint8_t unitStats[16][3] PROGMEM = {
  {2,1,0}, {5,1,0}, {6,1,0},
  {5,1,1}, {2,1,1}, {7,1,2}, {11,1,3}, {6,3,1}, {9,4,1}, {8,4,0},
  {6,1,1}, {9,1,2}, {11,1,3},
  {8,1,1}, {9,4,3}, {0,0,1},
};
static uint8_t unitStat(uint8_t kind, uint8_t i) { return pgm_read_byte(&unitStats[kind][i]); }
static uint8_t unitReach(uint8_t kind) { return unitStat(kind, 1); }

// Who can hit whom: per attacker, one bit per defender, kind 0 in the top bit.
const uint16_t attackTable[16] PROGMEM = {
  0b1111111111100111, 0b1111111111100111, 0b1111111111000111,
  0b1111111111000111, 0b1111111111000111, 0b1111111111000111, 0b1111111111000111,
  0b1111111111000111, 0b0000000000111111, 0b1111111111000111,
  0b1111111111111111, 0b0000000000111000, 0b1111111111111111,
  0b0000000000111111, 0b1111111111111111, 0b0000000000000000,
};

constexpr uint8_t FullHealth = 10;
constexpr uint8_t FortifyBonus = 2;   // extra defense behind sandbags, the same as a hill

// Cover on tile (c, r) for ground units and ships: HQ 3, other buildings 2, mountain 3, hill 2, forest 1.
static uint8_t coverAt(uint8_t c, uint8_t r) {
  FX::seekData(mapFirstBuilding + c + r);
  uint8_t i = FX::readEnd();
  FX::seekData(mapBuildings + i * 7);
  for (; i < buildingCount; i++) {
    uint8_t bc = FX::readPendingUInt8(), br = FX::readPendingUInt8(), kind = FX::readPendingUInt8() & 15;
    fxRead16();
    fxRead16();
    if (bc + br > c + r) break;
    if (bc == c && br == r) { FX::readEnd(); return kind ? 2 : 3; }
  }
  FX::readEnd();
  FX::seekData(mapGrid + r * mapW + c);
  uint8_t terrain = FX::readEnd() & 15;
  return terrain == 1 ? 1 : terrain == 6 ? 2 : terrain == 2 ? 3 : 0;
}

// The planned damage of `a` on `d`: attack scaled by health (rounded up), minus defense, cover and
// Fortify, at least 1. 0 when `a` can not hit `d`. Planes get no cover.
static uint8_t damage(const AWUnit &a, const AWUnit &d) {
  if (!(pgm_read_word(&attackTable[a.kind]) & (0x8000 >> d.kind))) return 0;
  uint8_t hit = (unitStat(a.kind, 0) * a.health + FullHealth - 1) / FullHealth;
  uint8_t block = unitStat(d.kind, 2) + (d.fortified ? FortifyBonus : 0);
  if (d.kind < 10 || d.kind > 12) block += coverAt(d.c, d.r);
  return hit > block ? hit - block : 1;
}

// A real hit: 1 less (1 in 4), as planned (2 in 4) or 1 more (1 in 4), never below 1.
// Multiply by 5, add 1: visits all 256 numbers before it repeats. startTurn mixes in the time of a key press.
uint8_t randomState;
static uint8_t rollDamage(uint8_t hit) {
  randomState = randomState * 5 + 1;
  uint8_t roll = randomState >> 6;
  if (roll == 0 && hit > 1) hit--;
  if (roll == 3) hit++;
  return hit;
}

static uint8_t distance(const AWUnit &a, const AWUnit &b) { return abs(a.c - b.c) + abs(a.r - b.r); }

// Moves left after leaving a tile with `left` moves: forests and hills cost 1 more,
// a mountain leaves at most 2, every step costs 1.
static int8_t leftAfter(uint8_t terrain, int8_t left) {
  if (terrain == 1 || terrain == 6) left--;
  else if (terrain == 2 && left > 2) left = 2;
  return left - 1;
}

// The range lives in a window around the unit, big enough for the longest move (7, plus 1 on a road).
// Per tile: one bit "reached" and 2 bits "came from" (grid direction of the step into it).
constexpr uint8_t RangeSize = 17, RangeHalf = 8;
constexpr uint16_t RangeTiles = RangeSize * RangeSize;
uint8_t rangeBits[(RangeTiles + 7) / 8];
uint8_t rangeFrom[(RangeTiles + 3) / 4];
int8_t rangeC, rangeR;        // grid tile of window tile (0, 0)
bool rangeOn;                 // draw the range
uint8_t selSide, selUnit;     // the selected unit
uint8_t startC, startR;       // where it stood
bool startFortified;          // fortified when selected: a walk takes the sandbags, Cancel gives them back

// Grid directions: c+1, r+1, c-1, r-1. On screen: down right, down left, up left, up right.
const int8_t dirC[4] PROGMEM = { 1, 0, -1, 0 };
const int8_t dirR[4] PROGMEM = { 0, 1, 0, -1 };

// The path from the selected unit to the cursor: grid directions, step by step.
constexpr uint8_t MaxPath = 9;
uint8_t pathDir[MaxPath], pathLength;
bool arrowOn;                 // draw the arrow
uint8_t pathC, pathR;         // the tile the arrow ends on

static bool inRange(uint8_t c, uint8_t r) {
  uint8_t wc = c - rangeC, wr = r - rangeR;
  if (wc >= RangeSize || wr >= RangeSize) return false;
  uint16_t i = wr * RangeSize + wc;
  return rangeBits[i / 8] & (1 << (i & 7));
}

// Who stands on a grid tile: 0 nobody, 1 player 1, 2 player 2.
static uint8_t unitOn(uint8_t c, uint8_t r) {
  for (uint8_t side = 0; side < 2; side++)
    for (uint8_t i = 0; i < match.unitCount[side]; i++)
      if (match.units[side][i].c == c && match.units[side][i].r == r) return side + 1;
  return 0;
}

// Flood the move range from the selected unit. Run between two layers: it borrows the screen buffer
// for the moves left per tile, who stands where and the terrain. The next layer draws over all of it.
// Tiles are taken in order of most moves left, so each tile spreads once.
static void floodRange() {
  AWUnit &u = match.units[selSide][selUnit];
  startC = u.c;
  startR = u.r;
  rangeC = int8_t(u.c) - RangeHalf;
  rangeR = int8_t(u.r) - RangeHalf;
  uint8_t *left = arduboy.getBuffer();
  uint8_t *who = left + RangeTiles;
  uint8_t *ground = who + RangeTiles;
  memset(left, 0, 2 * RangeTiles);
  memset(ground, 15, RangeTiles);   // off the map: nobody may go there
  for (uint8_t wr = 0; wr < RangeSize; wr++) {
    int8_t r = rangeR + wr;
    if (r < 0 || r >= mapH) continue;
    int8_t c0 = max(rangeC, 0), c1 = min(rangeC + RangeSize, mapW);
    FX::seekData(mapGrid + r * mapW + c0);
    for (int8_t c = c0; c < c1; c++) ground[wr * RangeSize + c - rangeC] = FX::readPendingUInt8() & 15;
    FX::readEnd();
  }
  for (uint8_t side = 0; side < 2; side++)
    for (uint8_t i = 0; i < match.unitCount[side]; i++) {
      uint8_t wc = match.units[side][i].c - rangeC, wr = match.units[side][i].r - rangeR;
      if (wc < RangeSize && wr < RangeSize) who[wr * RangeSize + wc] = side + 1;
    }
  uint16_t start = RangeHalf * RangeSize + RangeHalf;
  uint8_t moves = pgm_read_byte(&unitMove[u.kind]) + 1;
  if (ground[start] == 5) moves++;   // starting on a road gives one more
  left[start] = moves;
  uint8_t allowed = terrainBits(u.kind);
  memset(rangeFrom, 0, sizeof(rangeFrom));
  for (uint8_t m = moves; m > 0; m--)
    for (uint16_t i = 0; i < RangeTiles; i++) {
      if (left[i] != m) continue;
      int8_t l = leftAfter(ground[i], m);
      if (l <= 0) continue;
      uint8_t wc = i % RangeSize, wr = i / RangeSize;
      for (uint8_t d = 0; d < 4; d++) {
        uint8_t nc = wc + pgm_read_byte(&dirC[d]), nr = wr + pgm_read_byte(&dirR[d]);
        if (nc >= RangeSize || nr >= RangeSize) continue;
        uint16_t n = nr * RangeSize + nc;
        // own units can be passed, the other side's units block the way
        if (l <= left[n] || ground[n] > 6 || !(allowed & (1 << ground[n])) || who[n] == 2 - selSide) continue;
        left[n] = l;
        rangeFrom[n / 4] = (rangeFrom[n / 4] & ~(3 << (n & 3) * 2)) | d << (n & 3) * 2;
      }
    }
  // Reached tiles. Own units inside stay inside the outline, but the unit can not stop on them (canStop).
  memset(rangeBits, 0, sizeof(rangeBits));
  for (uint16_t i = 0; i < RangeTiles; i++)
    if (left[i]) rangeBits[i / 8] |= 1 << (i & 7);
  memset(left, 0, 3 * RangeTiles);
  rangeOn = true;
}

// The unit may end its move on a reached tile where nobody stands, or where it stood.
static bool canStop(uint8_t c, uint8_t r) {
  return inRange(c, r) && ((c == startC && r == startR) || !unitOn(c, r));
}

// Enemy unit i is a target of the selected unit: in reach and it can be hit.
static bool isTarget(uint8_t i) {
  AWUnit &u = match.units[selSide][selUnit], &e = match.units[selSide ^ 1][i];
  return distance(u, e) <= unitReach(u.kind) && damage(u, e);
}

// Marks the targets in the range bits, on a window around the unit (reach 4 fits). Returns how many.
// A unit with reach over 1 can not move and attack in the same turn.
static uint8_t markTargets() {
  AWUnit &u = match.units[selSide][selUnit];
  rangeC = int8_t(u.c) - RangeHalf;
  rangeR = int8_t(u.r) - RangeHalf;
  memset(rangeBits, 0, sizeof(rangeBits));
  if (unitReach(u.kind) > 1 && (u.c != startC || u.r != startR)) return 0;
  uint8_t n = 0;
  for (uint8_t i = 0; i < match.unitCount[selSide ^ 1]; i++) {
    if (!isTarget(i)) continue;
    AWUnit &e = match.units[selSide ^ 1][i];
    uint16_t t = uint8_t(e.r - rangeR) * RangeSize + uint8_t(e.c - rangeC);
    rangeBits[t / 8] |= 1 << (t & 7);
    n++;
  }
  return n;
}

// The path to tile (c, r), walked back along "came from". No arrow when the unit can not stop there.
static void buildPath(uint8_t c, uint8_t r) {
  pathC = c;
  pathR = r;
  arrowOn = false;
  if (!canStop(c, r) || (c == startC && r == startR)) return;
  uint8_t n = 0;
  while (c != startC || r != startR) {
    uint16_t i = uint8_t(r - rangeR) * RangeSize + uint8_t(c - rangeC);
    uint8_t d = rangeFrom[i / 4] >> (i & 3) * 2 & 3;
    pathDir[n++] = d;
    c -= pgm_read_byte(&dirC[d]);
    r -= pgm_read_byte(&dirR[d]);
  }
  for (uint8_t i = 0; i < n / 2; i++) { uint8_t t = pathDir[i]; pathDir[i] = pathDir[n - 1 - i]; pathDir[n - 1 - i] = t; }
  pathLength = n;
  arrowOn = true;
}

// ---- Drawing, each layer, in screen pixels ----

// Bit masks: from bit i up, and from bit 0 up to bit i.
const uint8_t maskFrom[8] PROGMEM = { 0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80 };
const uint8_t maskTo[8] PROGMEM = { 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };
const uint8_t twoBits[8] PROGMEM = { 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80 };   // bits i and i+1

// Rows y0 to y1 of screen column x in this layer: set the bits of `pattern` (white) or clear them.
AW_FAST static void columnBits(int16_t x, int16_t y0, int16_t y1, bool set, uint8_t pattern = 0xff) {
  if (x < 0 || x >= WIDTH || y1 < 0 || y0 >= HEIGHT) return;
  if (y0 < 0) y0 = 0;
  if (y1 >= HEIGHT) y1 = HEIGHT - 1;
  uint8_t *b = arduboy.getBuffer() + x + (y0 >> 3) * WIDTH;
  uint8_t pages = (y1 >> 3) - (y0 >> 3);
  uint8_t m = pgm_read_byte(&maskFrom[y0 & 7]) & pattern, last = pgm_read_byte(&maskTo[y1 & 7]);
  for (; pages; pages--, b += WIDTH, m = pattern)
    if (set) *b |= m; else *b &= ~m;
  m &= last;
  if (set) *b |= m; else *b &= ~m;
}

static void columnRun(int16_t x, int16_t y0, int16_t y1, bool set) { columnBits(x, y0, y1, set); }

// The range: reached ground gets a light gray checker (white pixels only, so only the white layer
// changes), and a black outline with marching dashes runs around the whole area.
// Drawn by screen columns. A screen column crosses two stacks of tiles (c-r = q on their left half,
// q-1 on their right half), one diamond under the other, c+r going up by 1 each. Reached tiles one
// under the other make one run of rows: fill it, and its first and last 2 rows are the outline.
// The tiles in a column change only every 24 columns, so the runs are found once per group.
AW_FAST static void drawRange(uint8_t plane, uint8_t phase) {
  int16_t sLo = (camY - 40) / 12, sHi = (camY + HEIGHT - 16) / 12;
  for (uint8_t x = 0; x < WIDTH;) {
    int16_t rel = camX + x - originX;
    int16_t q = rel >= 0 ? rel / 24 : -((23 - rel) / 24);
    uint8_t k = rel - q * 24;
    uint8_t n = min(24 - k, WIDTH - x);
    // runs of reached tiles: screen row of the first diamond's middle and of the last one's,
    // and whether each sits in stack q-1 (right half) or q (left half)
    int16_t runTop[4], runBottom[4];
    uint8_t runSide = 0, runs = 0;
    bool open = false;
    int16_t row = sLo * 12 + 28 - camY;
    for (int16_t sum = sLo; sum <= sHi + 1; sum++, row += 12) {
      bool odd = (sum - q) & 1;
      int16_t d = odd ? q - 1 : q;
      bool reached = sum <= sHi && inRange((sum + d) >> 1, (sum - d) >> 1);
      if (reached && !open && runs < 4) { runTop[runs] = row; runSide |= odd << (2 * runs); open = true; }
      if (!reached && open) { runBottom[runs] = row - 12; runSide |= !odd << (2 * runs + 1); runs++; open = false; }
    }
    // Each run, column by column. Half heights: left half of stack q grows by 1 every 2 columns,
    // right half of stack q-1 shrinks by 1 every 2 columns.
    uint8_t *col0 = arduboy.getBuffer() + x;
    for (uint8_t i = 0; i < runs; i++, runSide >>= 2) {
      uint8_t *col = col0;
      uint8_t kk = k;
      for (uint8_t xx = x; xx < x + n; xx++, kk++, col++) {
        uint8_t hq = (kk + 1) >> 1, hq1 = (24 - kk) >> 1;
        int16_t top = runTop[i] - (runSide & 1 ? hq1 : hq);
        int16_t bottom = runBottom[i] + (runSide & 2 ? hq1 : hq);
        if (bottom < 0 || top >= HEIGHT) continue;
        uint8_t t = top < 0 ? 0 : top, b = bottom >= HEIGHT ? HEIGHT - 1 : bottom;
        if (plane == 2) {
          uint8_t checker = (xx + camX + camY) & 1 ? 0xaa : 0x55;
          uint8_t *p = col + (t & 0xf8) * 16;
          uint8_t m = pgm_read_byte(&maskFrom[t & 7]) & checker;
          for (uint8_t pages = (b >> 3) - (t >> 3); pages; pages--, p += WIDTH, m = checker) *p &= ~m;
          *p &= ~(m & pgm_read_byte(&maskTo[b & 7]));
        }
        if (((xx + camX + phase) & 7) >= 4) continue;
        // 2 outline rows at each end of the run, where the run really ends on screen
        if (top >= 0) {
          uint8_t *p = col + (t & 0xf8) * 16;
          *p &= ~pgm_read_byte(&twoBits[t & 7]);
          if ((t & 7) == 7 && t < HEIGHT - 1) p[WIDTH] &= ~1;
        }
        if (bottom < HEIGHT && b > 0) {
          uint8_t u = b - 1;
          uint8_t *p = col + (u & 0xf8) * 16;
          *p &= ~pgm_read_byte(&twoBits[u & 7]);
          if ((u & 7) == 7) p[WIDTH] &= ~1;
        }
      }
    }
    x += n;
  }
}

// The arrow: straight parts column by column, corners and the head from ArrowArt.h.
// All black runs first, then all white runs, then light gray dashes that run toward the target.
AW_FAST static void drawArrow(uint8_t plane, uint8_t phase) {
  for (uint8_t pass = 0; pass < 3; pass++) {
    uint8_t c = startC, r = startR;
    for (uint8_t i = 0; i < pathLength; i++) {
      uint8_t d = pathDir[i];
      int8_t sx = d == 0 || d == 3 ? 1 : -1, sy = d < 2 ? 1 : -1;
      int16_t ax = tileX(c, r) - camX + 24, ay = tileY(c, r) - camY + 28;
      bool last = i == pathLength - 1;
      uint8_t k0 = i > 0 && pathDir[i - 1] != d ? CornerCols : 0;
      uint8_t k1 = last ? 24 - HeadCols : pathDir[i + 1] != d ? 24 - CornerCols : 24;
      for (uint8_t k = k0; k <= k1; k++) {
        int16_t x = ax + sx * k, y = ay + sy * (k >> 1);
        if (x < 0 || x >= WIDTH) continue;
        if (pass == 0) {
          int16_t ya = ay + sy * (int8_t(k - 1) >> 1), yb = ay + sy * ((k + 1) >> 1);
          columnRun(x, min(ya, yb) - 2, max(ya, yb) + 2, false);
        } else if (pass == 1) columnRun(x, y - 1, y + 1, true);
        else if (plane == 2 && uint8_t(i * 24 + k - phase) % 8 < 3) columnBits(x, y, y, false);
      }
      c += pgm_read_byte(&dirC[d]);
      r += pgm_read_byte(&dirR[d]);
      if (pass == 2) continue;
      uint8_t p = last ? 16 + d : pathDir[i + 1] != d ? d * 4 + pathDir[i + 1] : 255;
      if (p == 255) continue;
      int16_t cx = tileX(c, r) - camX + 24, cy = tileY(c, r) - camY + 28;
      uint16_t from = pgm_read_word(&arrowPieces[p][pass]);
      uint16_t to = pass ? pgm_read_word(&arrowPieces[p + 1][0]) : pgm_read_word(&arrowPieces[p][1]);
      for (const int8_t *run = arrowRuns + from * 3; run < arrowRuns + to * 3; run += 3)
        columnRun(cx + int8_t(pgm_read_byte(run)), cy + int8_t(pgm_read_byte(run + 1)), cy + int8_t(pgm_read_byte(run + 2)), pass);
    }
  }
}
