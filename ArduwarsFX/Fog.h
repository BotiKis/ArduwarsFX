#pragma once
// Fog of war, as the side whose turn it is sees it (hot seat). 1 bit per tile, 1 = fogged.
// Fog halves the brightness and keeps the order of the shades. Every other pixel of a fogged tile is
// blanked in layer 0, the pixels between them in layer 1: white turns into a fine checker of dark gray
// and light gray, light gray into light gray and black, dark gray into dark gray and black.
// Blanking one whole layer does not work with 2 layers: without the bright layer, light gray would
// end up darker than dark gray.
// The fogged tiles are filled by screen columns, like the move range: drawTiles in Rules.h.
// Who sees what: computeFog in Rules.h.

uint8_t fogBits[MaxMapSize * MaxMapSize / 8];   // bit r*32 + c

const uint8_t bitOf[8] PROGMEM = { 1, 2, 4, 8, 16, 32, 64, 128 };
const uint8_t maskFrom[8] PROGMEM = { 0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80 };   // from bit i up

// Called a lot per layer: kept small and inline. Off the map counts as fog.
static bool isFog(int8_t c, int8_t r) {
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

// The pixels the fog blanks in screen column x of a layer: a checker that sticks to the map.
static uint8_t fogPattern(uint8_t x, uint8_t plane) {
  uint8_t pattern = (x + camX + camY) & 1 ? 0xaa : 0x55;
  return plane ? ~pattern : pattern;
}

// One byte from the FX chip, inline (FX::readPendingUInt8 is a call).
static inline __attribute__((always_inline)) uint8_t fxByte() {
  while (!(SPSR & _BV(SPIF))) {}
  uint8_t v = SPDR;
  SPDR = 0;
  return v;
}

// Fogs the pixels under the mask of a picture on the FX chip (w wide, pages of 8 rows, a mask byte
// after each picture byte) at screen x, y: a top in fog, drawn before.
AW_FAST static void fogMask(int16_t x, int16_t y, uint8_t w, uint8_t pages, uint24_t addr, uint8_t plane) {
  int16_t c0 = x < 0 ? -x : 0, c1 = min(int16_t(w), int16_t(WIDTH - x));
  if (c0 >= c1) return;
  uint8_t n = c1 - c0, mul = 1 << (y & 7);
  int8_t page = y >> 3;
  uint8_t *start = arduboy.getBuffer() + x + c0;
  uint8_t pattern0 = fogPattern(x + c0, plane);
  for (uint8_t p = 0; p < pages; p++, page++) {
    bool lo = page >= 0 && page < 8, hi = mul != 1 && page >= -1 && page < 7;
    if (!lo && !hi) continue;
    uint8_t *b = start + page * 128;
    uint8_t pattern = pattern0;
    FX::seekData(addr + (uint16_t(p) * w + c0) * 2);
    for (uint8_t i = n; i; i--, b++, pattern = ~pattern) {
      fxByte();
      uint8_t m = fxByte();
      if (!m) continue;
      uint16_t v = m * mul;
      if (lo) b[0] &= ~(uint8_t(v) & pattern);
      if (hi) b[128] &= ~(uint8_t(v >> 8) & pattern);
    }
    FX::readEnd();
  }
}
