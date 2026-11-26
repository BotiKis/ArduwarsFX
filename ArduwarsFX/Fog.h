#pragma once
// Fog of war, as the side whose turn it is sees it (hot seat). 1 bit per tile, 1 = fogged.
// A fogged pixel is 2 shades darker: it is blanked in layers 0 and 1, layer 2 (the white pixels) stays.
// So fog needs no darkened copy of the map picture. The edge of the sight fades: dithered masks
// on the tile corners (Wang tiles, fogWang on the FX chip, made by tools/fogwang.py).
// A 64x64 screen block with only fog around it is not drawn in layers 0 and 1 at all.
// Who sees what: computeFog in Rules.h.

uint8_t fogBits[MaxMapSize * MaxMapSize / 8];   // bit r*32 + c
uint8_t darkBlocks;       // bit br*3 + bc: screen block with only fog around it
bool fogChanged = true;   // the dark blocks are found again

const uint8_t bitOf[8] PROGMEM = { 1, 2, 4, 8, 16, 32, 64, 128 };
const uint8_t maskFrom[8] PROGMEM = { 0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80 };   // from bit i up

// Called a lot per layer: kept small and inline. Off the map counts as fog.
static inline __attribute__((always_inline)) bool isFog(int8_t c, int8_t r) {
  if (uint8_t(c) >= mapW || uint8_t(r) >= mapH) return true;
  return fogBits[uint8_t(r) * 4 + (uint8_t(c) >> 3)] & pgm_read_byte(bitOf + (c & 7));
}

// Any tile in the 3x3 tiles behind (c, r) (up the screen) with the given fog state.
static bool behindIs(uint8_t c, uint8_t r, bool fog) {
  for (int8_t dr = -2; dr <= 0; dr++)
    for (int8_t dc = -2; dc <= 0; dc++)
      if ((dr || dc) && isFog(c + dc, r + dr) == fog) return true;
  return false;
}

static int16_t floorDiv48(int16_t a) { return (a >= 0 ? a : a - 47) / 48; }

// True when every tile that touches the map picture area x0..x1, y0..y1 is fogged.
// Undo the projection: a = 2dy+dx, b = 2dy-dx from the top corner of tile (0, 0) give c = a/48, r = b/48.
static bool allFog(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  x0 -= originX + 24; x1 -= originX + 24; y0 -= 16; y1 -= 16;
  int16_t cLo = max(floorDiv48(2 * y0 + x0), -1), cHi = min(floorDiv48(2 * y1 + x1), mapW);
  int16_t rLo = max(floorDiv48(2 * y0 - x1), -1), rHi = min(floorDiv48(2 * y1 - x0), mapH);
  for (int8_t r = rLo; r <= rHi; r++)
    for (int8_t c = cLo; c <= cHi; c++)
      if (!isFog(c, r)) return false;
  return true;
}

// The screen blocks with only fog around them, when the blocks or the fog changed.
static void prepareFogBlocks() {
  static uint8_t lastC = 255, lastR;
  if (!fogChanged && blockC == lastC && blockR == lastR) return;
  lastC = blockC;
  lastR = blockR;
  fogChanged = false;
  darkBlocks = 0;
  for (uint8_t br = 0; br < 2; br++)
    for (uint8_t bc = 0; bc < 3; bc++) {
      int16_t x = (blockC + bc) * 64, y = (blockR + br) * 64;
      // a margin of half a tile: a Wang edge reaches that far
      if (allFog(x - 24, y - 12, x + 64 + 23, y + 64 + 11)) darkBlocks |= 1 << (br * 3 + bc);
    }
}

static bool darkAt(int16_t x, int16_t y) {
  x = constrain(x, 0, WIDTH - 1);
  y = constrain(y, 0, HEIGHT - 1);
  uint8_t bc = (x + camX) / 64 - blockC, br = (y + camY) / 64 - blockR;
  return darkBlocks & (1 << (br * 3 + bc));
}

// Blanks one ground diamond (48x24, the same split as the cursor) with its top corner at screen x, y.
AW_FAST static void clearCell(int16_t x, int16_t y) {
  int8_t dx0 = x < 23 ? -x : -23, dx1 = x > WIDTH - 24 ? WIDTH - 1 - x : 23;
  uint8_t *col = arduboy.getBuffer() + x + dx0;
  for (int8_t dx = dx0; dx <= dx1; dx++, col++) {
    uint8_t a = abs(dx);
    int16_t y0 = y + ((a + 1) >> 1), y1 = y + ((47 - a) >> 1);
    if (y0 < 0) y0 = 0;
    if (y1 > HEIGHT - 1) y1 = HEIGHT - 1;
    if (y0 > y1) continue;
    uint8_t p0 = y0 >> 3, p1 = y1 >> 3;
    uint8_t m0 = pgm_read_byte(maskFrom + (y0 & 7)), m1 = ~uint8_t(pgm_read_byte(maskFrom + (y1 & 7)) << 1);
    uint8_t *b = col + p0 * 128;
    if (p0 == p1) { *b &= ~(m0 & m1); continue; }
    *b &= ~m0;
    for (uint8_t p = p0 + 1; p < p1; p++) { b += 128; *b = 0; }
    b[128] &= ~m1;
  }
}

// One byte from the FX chip, inline (FX::readPendingUInt8 is a call).
static inline __attribute__((always_inline)) uint8_t fxByte() {
  while (!(SPSR & _BV(SPIF))) {}
  uint8_t v = SPDR;
  SPDR = 0;
  return v;
}

// Blanks the set bits of a mask on the FX chip (w wide, pages of 8 rows) at screen x, y.
// Stride 2: a picture with its mask after each picture byte (the tops in fog).
AW_FAST static void clearMask(int16_t x, int16_t y, uint8_t w, uint8_t pages, uint24_t addr, uint8_t stride) {
  int16_t c0 = x < 0 ? -x : 0, c1 = min(int16_t(w), int16_t(WIDTH - x));
  if (c0 >= c1) return;
  uint8_t n = c1 - c0, mul = 1 << (y & 7);
  int8_t page = y >> 3;
  uint8_t *start = arduboy.getBuffer() + x + c0;
  for (uint8_t p = 0; p < pages; p++, page++) {
    bool lo = page >= 0 && page < 8, hi = mul != 1 && page >= -1 && page < 7;
    if (!lo && !hi) continue;
    uint8_t *b = start + page * 128;
    FX::seekData(addr + (uint16_t(p) * w + c0) * stride);
    for (uint8_t i = n; i; i--, b++) {
      if (stride == 2) fxByte();
      uint8_t m = fxByte();
      if (!m) continue;
      uint16_t v = m * mul;
      if (lo) b[0] &= ~uint8_t(v);
      if (hi) b[128] &= ~uint8_t(v >> 8);
    }
    FX::readEnd();
  }
}

// One fog edge mask (fogWang, tools/fogwang.py) with the cell's top corner at screen x, y. The mask is
// stored column by column, only the pages each column of the diamond touches: one read from the FX chip.
constexpr uint8_t FogMaskSize = 107;
AW_FAST static void fadeCell(int16_t x, int16_t y, uint24_t addr) {
  FX::seekData(addr);
  uint8_t mul = 1 << (y & 7);
  int8_t page0 = y >> 3;
  int16_t sx = x - 23;
  uint8_t *col = arduboy.getBuffer() + sx;
  for (uint8_t i = 1; i < 48; i++, sx++, col++) {   // column 0 of the cell is empty
    uint8_t a = i < 24 ? 24 - i : i - 24;
    uint8_t p0 = (a + 1) >> 4, p1 = (47 - a) >> 4;   // pages of rows (a+1)/2 to (47-a)/2
    if (sx < 0 || sx >= WIDTH) {
      for (uint8_t p = p0; p <= p1; p++) fxByte();
      continue;
    }
    // each mask byte lands in two screen pages: its low part, and its high part in the page below
    uint8_t page = page0 + p0, carry = 0;
    uint8_t *b = col + int8_t(page) * 128;
    for (uint8_t p = p0; p <= p1; p++, page++, b += 128) {
      uint16_t v = fxByte() * mul;
      uint8_t m = uint8_t(v) | carry;
      carry = v >> 8;
      if (m && page < 8) *b &= ~m;
    }
    if (carry && page < 8) *b &= ~carry;
  }
  FX::readEnd();
}

// The tile corners on screen and what each needs, found once per picture (prepareFog) instead of in
// every layer. Corner (i, j) is the top corner of tile (i, j), by diagonal s = c+r and column d = c-r.
// Its cell is a ground diamond 12 pixels higher. Cell code N + 2E + 4S + 8W, 1 = fogged, 0 = nothing
// to draw (no fog, or inside a dark block). 4 bits per cell.
constexpr uint8_t MaxFogCells = 48;
uint8_t fogCells[MaxFogCells / 2];
int16_t fogOX, fogOY;     // screen position of corner (0, 0) when the cells were found
int8_t fogSLo, fogSHi, fogDLo, fogDHi;

static void prepareFog() {
  static int16_t lastX = -1000, lastY;
  bool changed = fogChanged;
  prepareFogBlocks();
  if (!changed && camX == lastX && camY == lastY) return;
  lastX = camX;
  lastY = camY;
  int16_t ox = fogOX = originX + 24 - camX, oy = fogOY = 4 - camY;
  fogSLo = -floorDiv48(4 * (oy + 23));   // y = s*12 + oy, 24 tall
  fogSHi = floorDiv48(4 * (63 - oy));
  fogDLo = -floorDiv48(2 * (ox + 23));   // x = d*24 + ox, 47 wide
  fogDHi = floorDiv48(2 * (WIDTH + 22 - ox));
  memset(fogCells, 0, sizeof(fogCells));
  uint8_t n = 0;
  for (int8_t s = fogSLo; s <= fogSHi; s++) {
    int16_t y = s * 12 + oy;
    for (int8_t d = fogDLo + ((fogDLo ^ s) & 1); d <= fogDHi && n < MaxFogCells; d += 2, n++) {
      int16_t x = d * 24 + ox;
      int8_t c = (s + d) >> 1, r = (s - d) >> 1;
      uint8_t f = isFog(c - 1, r - 1) | isFog(c, r - 1) << 1 | isFog(c, r) << 2 | isFog(c - 1, r) << 3;
      if (darkAt(x - 23, y) && darkAt(x + 23, y) && darkAt(x - 23, y + 23) && darkAt(x + 23, y + 23)) f = 0;
      fogCells[n >> 1] |= f << (n & 1) * 4;
    }
  }
}

// Layers 0 and 1: blank the fog over the terrain, before the draw list.
AW_FAST static void drawFog() {
  uint8_t n = 0;
  for (int8_t s = fogSLo; s <= fogSHi; s++) {
    int16_t y = s * 12 + fogOY;
    for (int8_t d = fogDLo + ((fogDLo ^ s) & 1); d <= fogDHi && n < MaxFogCells; d += 2, n++) {
      uint8_t f = fogCells[n >> 1] >> (n & 1) * 4 & 15;
      if (!f) continue;
      int16_t x = d * 24 + fogOX;
      if (f == 15) clearCell(x, y);
      else fadeCell(x, y, fogWang + (f - 1) * FogMaskSize);
    }
  }
}
