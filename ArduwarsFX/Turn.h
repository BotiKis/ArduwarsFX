#pragma once
// A turn, hot seat for 2 players on one device. Buttons: B confirms, A goes back.
// - Free cursor: B on an own unit that can still act selects it, B anywhere else opens End Turn.
// - Selected: the range and the arrow show. B on a tile in range walks there (or stays), then the unit
//   menu: Wait ends the unit's turn, Cancel (or A) puts it back. A drops the selection.
// - End Turn: "N LEFT" jumps to a unit that can still act. END TURN wipes the map out, shows the
//   next player on black, and wipes in that player's view.
// Boxes, the walk and the wipe run their own loops: draw a layer, send it, think once per picture.
#include <Tinyfont.h>         // only for the library: the letters come from TinyfontSprite.c
#include <TinyfontSprite.c>
#include "Localization.h"

enum class AWTurn : uint8_t { Free, Selected };
AWTurn turn = AWTurn::Free;
bool sceneBlack;          // boxes and the wipe draw black behind them instead of the map

// ---- Text and boxes ----

int16_t printX, printY;

// One Tinyfont letter at printX, printY in black, then printX moves on. Two letters share 4 bytes,
// one byte per column: the even letter in the low 4 bits, the odd one in the high 4 bits.
static void printChar(char c) {
  const uint8_t *letter = TINYFONT_SPRITE + ((c - 32) / 2) * 4;
  uint8_t *column = arduboy.getBuffer() + printX + (printY / 8) * WIDTH;
  uint8_t shift = printY % 8;
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t bits = pgm_read_byte(letter + i);
    uint16_t pixels = uint16_t(c & 1 ? bits >> 4 : bits & 15) << shift;
    column[0] &= ~pixels;
    if (shift > 4) column[WIDTH] &= ~(pixels >> 8);
    column++;
  }
  printX += 5;
}

static void printAt(int16_t x, int16_t y, char_P *text) {
  printX = x;
  printY = y;
  char c;
  while ((c = pgm_read_byte(text++)) != 0) printChar(c);
}

static void printNumber(int16_t x, int16_t y, uint8_t n) {
  printX = x;
  printY = y;
  if (n >= 10) printChar('0' + n / 10);
  printChar('0' + n % 10);
}

// A box with rounded corners in a shade (BLACK to WHITE), as in the old game.
static void fillBox(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t shade) {
  uint8_t lit = shade > arduboy.currentPlane();
  for (uint8_t i = 0; i < 3; i++) {
    uint8_t inset = 3 - i;
    SpritesU::fillRect(x + i, y + inset, 1, h - 2 * inset, lit);
    SpritesU::fillRect(x + w - 1 - i, y + inset, 1, h - 2 * inset, lit);
  }
  SpritesU::fillRect(x + 3, y, w - 6, h, lit);
}

static void drawScene() {
  if (sceneBlack) memset(arduboy.getBuffer(), 0, WIDTH * HEIGHT / 8);
  else drawMap();
}

// A dialog (one line in the middle, B closes it) or a list of options at the top right with a cursor.
// Returns the chosen option, -1 when A closed the list. `number` goes in front of the first option.
static int8_t showBox(char_P * const options[], uint8_t count, bool isDialog, uint8_t number = 0) {
  uint8_t w = isDialog ? strlen_P(options[0]) * 5 + 8 : 54, h = isDialog ? 16 : 6 + count * 7;
  uint8_t x = isDialog ? (WIDTH - w) / 2 : WIDTH - w - 4, y = isDialog ? 24 : 10;
  uint8_t textX = x + (isDialog ? 4 : 9), textY = y + (isDialog ? 6 : 4);
  int8_t chosen = 0;
  while (true) {
    drawScene();
    fillBox(x, y, w, h, BLACK);
    fillBox(x + 1, y + 1, w - 2, h - 3, WHITE);
    for (uint8_t i = 0; i < count; i++) {
      uint8_t tx = textX;
      if (i == 0 && number) { printNumber(tx, textY, number); tx = printX; }
      printAt(tx, textY + 7 * i, options[i]);
    }
    if (!isDialog) printAt(x + 4, textY + 7 * chosen, LOCA_cursor);
    if (!sendLayer()) continue;
    if (arduboy.justPressed(B_BUTTON)) return chosen;
    if (isDialog) continue;
    if (arduboy.justPressed(A_BUTTON)) return -1;
    if (arduboy.justPressed(DOWN_BUTTON)) chosen = chosen + 1 == count ? 0 : chosen + 1;
    if (arduboy.justPressed(UP_BUTTON)) chosen = chosen ? chosen - 1 : count - 1;
  }
}

// The wipe from the old game: rows of 8 pixels slide sideways, even rows one way, odd rows the other,
// 6 pixels per picture. Out: the scene slides away to black. In: it slides in from black.
static void wipe(bool in) {
  uint8_t *buf = arduboy.getBuffer();
  for (uint8_t step = 0; step < 22;) {
    drawScene();
    uint8_t shift = in ? 128 - 6 * step : 6 * (step + 1);
    if (shift > 128) shift = 128;
    for (uint8_t y = 0; y < 8; y++) {
      uint8_t *row = buf + y * WIDTH;
      if (y % 2 == 0) { memmove(row, row + shift, WIDTH - shift); memset(row + WIDTH - shift, 0, shift); }
      else { memmove(row + shift, row, WIDTH - shift); memset(row, 0, shift); }
    }
    if (sendLayer()) step++;
  }
}

// ---- The turn ----

static uint8_t readyUnits() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < match.unitCount[match.side]; i++) n += !match.units[match.side][i].moved;
  return n;
}

static void startTurn() {
  sceneBlack = true;
  char_P *name = match.side ? LOCA_player2 : LOCA_player1;
  showBox(&name, 1, true);
  camX = match.viewX[match.side];
  camY = match.viewY[match.side];
  mapChanged = true;
  prepareBlocks();
  updateMapView();
  sceneBlack = false;
  wipe(true);
}

static void endTurn() {
  // The units that acted stand ready again, so the other player sees them as usual.
  for (uint8_t i = 0; i < match.unitCount[match.side]; i++) match.units[match.side][i].moved = 0;
  mapChanged = true;
  match.viewX[match.side] = camX;
  match.viewY[match.side] = camY;
  wipe(false);
  sceneBlack = true;
  match.side ^= 1;
  if (match.side == 0) match.day++;
  startTurn();
}

static void dropSelection() {
  turn = AWTurn::Free;
  rangeOn = arrowOn = false;
  cursorShown = true;
  prepareCursor();
}

// The selected unit walks along the arrow, 6 pictures per tile, the camera glides after it.
static void walkPath() {
  AWUnit &u = match.units[selSide][selUnit];
  walking = true;
  walkSide = selSide;
  walkUnit = selUnit;
  rangeOn = arrowOn = cursorShown = false;
  mapChanged = true;
  for (uint8_t i = 0, t = 0; i < pathLength;) {
    drawMap();
    if (!sendLayer()) continue;
    uint8_t d = pathDir[i];
    uint8_t nc = u.c + pgm_read_byte(&dirC[d]), nr = u.r + pgm_read_byte(&dirR[d]);
    // drawn in the depth of whichever tile is further in front
    walkC = d < 2 ? nc : u.c;
    walkR = d < 2 ? nr : u.r;
    t++;
    int8_t sx = d == 0 || d == 3 ? 1 : -1, sy = d < 2 ? 1 : -1;
    walkX = tileX(u.c, u.r) + 12 + sx * 4 * t;
    walkY = tileY(u.c, u.r) + 10 + sy * 2 * t;
    if (t == 6) { u.c = nc; u.r = nr; t = 0; i++; }
    camX += (walkX + 12 - WIDTH / 2 - camX) / 4;
    camY += (walkY + 18 - HEIGHT / 2 - camY) / 4;
    updateMapView();
  }
  walking = false;
  mapChanged = true;
  centerOn(u.c, u.r);
  updateMapView();
}

static void unitMenu() {
  static char_P * const options[2] = { LOCA_wait, LOCA_cancel };
  rangeOn = arrowOn = cursorShown = false;
  prepareCursor();
  AWUnit &u = match.units[selSide][selUnit];
  if (showBox(options, 2, false) == 0) {
    u.moved = 1;
    mapChanged = true;
    dropSelection();
    return;
  }
  // Cancel: back to where it stood, still selected.
  u.c = startC;
  u.r = startR;
  mapChanged = true;
  centerOn(startC, startR);
  updateMapView();
  rangeOn = cursorShown = true;
  prepareCursor();
}

static void endTurnMenu() {
  static char_P * const options[3] = { LOCA_unitsLeft, LOCA_endTurn, LOCA_cancel };
  uint8_t ready = readyUnits(), first = ready ? 0 : 1;
  int8_t chosen = showBox(options + first, 3 - first, false, ready);
  if (chosen < 0) return;
  chosen += first;
  if (chosen == 1) endTurn();
  if (chosen == 0)
    for (uint8_t i = 0; i < match.unitCount[match.side]; i++)
      if (!match.units[match.side][i].moved) { centerOn(match.units[match.side][i].c, match.units[match.side][i].r); updateMapView(); break; }
}

// Once per picture on the map.
static void updateTurn() {
  uint8_t lastC = cursorC, lastR = cursorR;
  panCamera();
  updateMapView();
  if (turn == AWTurn::Selected && (cursorC != lastC || cursorR != lastR)) {
    buildPath(cursorC, cursorR);
    cursorShown = !arrowOn;
    prepareCursor();
  }
  if (arduboy.justPressed(A_BUTTON) && turn == AWTurn::Selected) dropSelection();
  if (!arduboy.justPressed(B_BUTTON)) return;
  if (turn == AWTurn::Selected) {
    if (!cursorOn || !canStop(cursorC, cursorR)) return;
    if (arrowOn) walkPath();
    unitMenu();
    return;
  }
  if (cursorOn)
    for (uint8_t i = 0; i < match.unitCount[match.side]; i++) {
      AWUnit &u = match.units[match.side][i];
      if (u.c != cursorC || u.r != cursorR || u.moved) continue;
      selSide = match.side;
      selUnit = i;
      floodRange();
      turn = AWTurn::Selected;
      arrowOn = false;
      return;
    }
  endTurnMenu();
}
