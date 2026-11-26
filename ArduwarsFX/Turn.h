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

enum class AWTurn : uint8_t { Free, Selected };
AWTurn turn = AWTurn::Free;
bool sceneBlack;          // boxes and the wipe draw black behind them instead of the map

// ---- Text and boxes ----

int16_t printX, printY;

// One Tinyfont letter at printX, printY in black, then printX moves on. Two letters share 4 bytes,
// one byte per column: the even letter in the low 4 bits, the odd one in the high 4 bits.
AW_FAST static void printChar(char c) {
  const uint8_t *letter = TINYFONT_SPRITE + ((c - 32) / 2) * 4;
  uint8_t *column = arduboy.getBuffer() + printX + (printY / 8) * WIDTH;
  uint8_t shift = printY % 8, down = pgm_read_byte(bitOf + shift);   // multiply instead of shifting: faster
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t bits = pgm_read_byte(letter + i);
    uint16_t pixels = uint8_t(c & 1 ? bits >> 4 : bits & 15) * down;
    column[0] &= ~pixels;
    if (shift > 4) column[WIDTH] &= ~(pixels >> 8);
    column++;
  }
  printX += 5;
}

// The health badge of a hurt unit at screen x, y: a black box of 6x7, its inside of 5x6 at the bottom
// right white, the digit in black. Straight into the buffer, the same in every layer.
// It may hang 2 pixels off the left or top edge and 1 off the bottom.
AW_FAST static void drawBadge(int16_t x, int16_t y, uint8_t health) {
  char c = '0' + health;
  const uint8_t *letter = TINYFONT_SPRITE + ((c - 32) / 2) * 4;
  uint8_t top = y < 0 ? 0 : y, cut = top - y, down = pgm_read_byte(bitOf + (top & 7));
  uint8_t *b = arduboy.getBuffer() + (top >> 3) * WIDTH + x;
  uint16_t clear = uint8_t(0x7f >> cut) * down;
  for (uint8_t i = 0; i < 6; i++, b++) {
    if (x + i < 0) continue;
    uint8_t white = i ? 0x7e : 0;   // column 0 is all black, the others are white below row 0
    if (i >= 2) {
      uint8_t bits = pgm_read_byte(letter + i - 2);
      white &= ~((c & 1 ? bits >> 4 : bits & 15) << 2);
    }
    uint16_t set = uint8_t(white >> cut) * down;
    b[0] = (b[0] & ~uint8_t(clear)) | uint8_t(set);
    if (top < HEIGHT - 8) b[WIDTH] = (b[WIDTH] & ~uint8_t(clear >> 8)) | uint8_t(set >> 8);
  }
}

// A text from the FX chip (fxdata/loca.txt) at x, y.
static void printAt(int16_t x, int16_t y, uint24_t text) {
  printX = x;
  printY = y;
  FX::seekData(text);
  char c;
  while ((c = FX::readPendingUInt8()) != 0) printChar(c);
  FX::readEnd();
}

static uint8_t textLength(uint24_t text) {
  FX::seekData(text);
  uint8_t n = 0;
  while (FX::readPendingUInt8()) n++;
  FX::readEnd();
  return n;
}

// The digits of n into p, at least `digits` of them. Returns the place after the last one.
static char *copyNumber(char *p, uint16_t n, uint8_t digits = 1) {
  char d[5];
  uint8_t i = 0;
  do { d[i++] = '0' + n % 10; n /= 10; } while (n || i < digits);
  while (i) *p++ = d[--i];
  return p;
}

static void printNumber(int16_t x, int16_t y, uint16_t n) {
  printX = x;
  printY = y;
  char digits[5];
  char *end = copyNumber(digits, n);
  for (char *p = digits; p < end; p++) printChar(*p);
}

// A box with rounded corners in a shade (BLACK to WHITE), as in the old game.
static void fillBox(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t shade) {
  uint8_t lit = shade >> arduboy.currentPlane() & 1;
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
static int8_t showBox(const uint24_t options[], uint8_t count, bool isDialog, uint8_t number = 0) {
  uint8_t w = isDialog ? textLength(options[0]) * 5 + 8 : 54, h = isDialog ? 16 : 6 + count * 7;
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
    if (!isDialog) { printX = x + 4; printY = textY + 7 * chosen; printChar('>'); }
    if (!sendLayer()) continue;
    if (arduboy.justPressed(B_BUTTON)) return chosen;
    if (isDialog) continue;
    if (arduboy.justPressed(A_BUTTON)) return -1;
    if (arduboy.justPressed(DOWN_BUTTON)) chosen = chosen + 1 == count ? 0 : chosen + 1;
    if (arduboy.justPressed(UP_BUTTON)) chosen = chosen ? chosen - 1 : count - 1;
  }
}

// One line in the middle of the screen, B closes it.
static void dialog(uint24_t text) { showBox(&text, 1, true); }

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

uint8_t winner;           // 1 player 1, 2 player 2, 3 a draw: the match is over

static uint8_t readyUnits() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < match.unitCount[match.side]; i++) n += !match.units[match.side][i].moved;
  return n;
}

// The top bar over the map, as in Arduwars 1.1: player, day, units that can still act of all units,
// money right-aligned. Its letters are found when the map changes (prepareTopBar, from updateMapView),
// each layer only draws them: 5 pixels per letter from x = 1.
constexpr uint8_t BarLetters = 25;
char barText[BarLetters];

static char *copyText(char *p, uint24_t text) {
  FX::seekData(text);
  char c;
  while ((c = FX::readPendingUInt8()) != 0) *p++ = c;
  FX::readEnd();
  return p;
}

static void prepareTopBar() {
  uint8_t side = match.side;
  memset(barText, ' ', BarLetters);
  char *p = copyText(barText, side ? LOCA_p2 : LOCA_p1) + 1;
  p = copyNumber(copyText(p, LOCA_day), match.day) + 1;
  p = copyNumber(p, readyUnits(), 2);
  *p++ = '/';
  copyNumber(p, match.unitCount[side], 2);
  char money[9];
  p = copyNumber(copyText(money, LOCA_money), match.money[side]);
  if (match.money[side]) p = copyText(p, LOCA_hundreds);
  memcpy(barText + BarLetters - (p - money), money, p - money);
}

// Straight into the top 8 rows of the buffer: rows 0-5 white, row 6 black, row 7 stays map.
// The letters sit on row 1: a Tinyfont column (4 bits) shifted down by 1.
AW_FAST static void drawTopBar() {
  uint8_t *b = arduboy.getBuffer();
  for (uint8_t x = 0; x < WIDTH; x++) b[x] = (b[x] & 0x80) | 0x3f;
  for (uint8_t i = 0; i < BarLetters; i++) {
    char c = barText[i];
    if (c == ' ') continue;
    const uint8_t *letter = TINYFONT_SPRITE + ((c - 32) >> 1) * 4;
    uint8_t *col = b + 1 + 5 * i;
    for (uint8_t j = 0; j < 4; j++) {
      uint8_t bits = pgm_read_byte(letter + j);
      col[j] &= ~((c & 1 ? bits >> 4 : bits & 15) << 1);
    }
  }
}

static void heal(AWUnit &u, uint8_t hp) { u.health = min(u.health + hp, FullHealth); }

// At the start of a turn, from Arduwars 1.1: a capture with nobody on the building starts over,
// each own city pays 2000, a unit on an own building of its kind heals 2 (planes at airports, ships at
// shipyards, land units at the others), each own Assist heals the own units next to it by 1.
static void upkeep() {
  uint8_t side = match.side;
  for (uint8_t i = 0; i < buildingCount; i++) {
    readBuilding(i);
    AWBuilding &b = match.buildings[i];
    if (!unitOn(bldC, bldR)) b.capture = fullCapture(bldKind);
    if (b.owner != side + 1) continue;
    if (bldKind == 1) match.money[side] = min(match.money[side] + 20, 999);
    uint8_t u = unitAt(side, bldC, bldR);
    if (u == 255) continue;
    uint8_t kind = match.units[side][u].kind;
    if (kind < 10 ? bldKind != 3 && bldKind != 4 : bldKind == (kind < 13 ? 3 : 4)) heal(match.units[side][u], 2);
  }
  for (uint8_t a = 0; a < match.unitCount[side]; a++) {
    if (match.units[side][a].kind != 4) continue;
    for (uint8_t i = 0; i < match.unitCount[side]; i++)
      if (distance(match.units[side][a], match.units[side][i]) == 1) heal(match.units[side][i], 1);
  }
}

static void startTurn() {
  sceneBlack = true;
  dialog(match.side ? LOCA_player2 : LOCA_player1);
  randomState += pictureCount;   // the damage roll depends on when B was pressed
  upkeep();
  computeFog();
  camX = match.viewX[match.side];
  camY = match.viewY[match.side];
  prepareBlocks();
  updateMapView();
  sceneBlack = false;
  wipe(true);
}

static void endTurn() {
  if (match.side && match.day == 255) { winner = 3; return; }   // after 255 days: a draw
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
    camY += (walkY + 18 - CenterY - camY) / 4;
    updateMapView();
  }
  walking = false;
  u.fortified = 0;   // walking leaves the sandbags behind
  centerOn(u.c, u.r);
  refreshMap();
}

// ---- Fights ----

// Pictures of the map for an effect: draw, send, n pictures.
static void showPictures(uint8_t n) {
  while (n) {
    drawMap();
    if (sendLayer()) n--;
  }
}

// A hit: the screen turns inverted for 4 pictures, then back for 4.
static void flash(uint8_t times) {
  while (times--)
    for (uint8_t on = 2; on-- > 0;) {
      FX::enableOLED();   // invert talks to the screen
      arduboy.invert(on);
      FX::disableOLED();
      showPictures(4);
    }
}

// The camera glides a quarter of the way to tile (c, r) each picture.
static void glideTo(uint8_t c, uint8_t r) {
  int16_t x = camX, y = camY;
  centerOn(c, r);
  int16_t dx = camX - x, dy = camY - y;
  camX = abs(dx) < 4 ? camX : x + dx / 4;
  camY = abs(dy) < 4 ? camY : y + dy / 4;
  updateMapView();
}

// Unit i of a side flickers for about a second, then it leaves the map.
static void destroyUnit(uint8_t side, uint8_t i) {
  for (uint8_t f = 0; f < 60;) {
    drawMap();
    if (!sendLayer()) continue;
    if (f++ % 10) continue;
    hideSide = (f / 10) % 2 ? 255 : side;
    hideUnit = i;
    refreshMap();
  }
  hideSide = 255;
  match.units[side][i] = match.units[side][--match.unitCount[side]];
  if (!match.unitCount[side]) winner = 2 - side;   // the last unit is gone: the other side wins
  refreshMap();
}

// The selected unit attacks enemy unit t. The cursor blinks on both for about a second, then the hit.
// Units with reach 1 next to each other: a defender that lives hits back.
static void fight(uint8_t t) {
  uint8_t enemy = selSide ^ 1;
  AWUnit &a = match.units[selSide][selUnit], &d = match.units[enemy][t];
  a.moved = 1;
  rangeOn = cursorShown = false;
  fightC[0] = d.c; fightR[0] = d.r;
  fightC[1] = a.c; fightR[1] = a.r;
  refreshMap();
  for (uint8_t f = 0; f < 60;) {
    drawMap();
    if (!sendLayer()) continue;
    if (f++ % 10) continue;
    fightOn = (f / 10) % 2 == 0;
    prepareCursor();
  }
  fightOn = false;
  prepareCursor();
  flash(3);
  uint8_t hit = rollDamage(damage(a, d));
  if (hit >= d.health) { destroyUnit(enemy, t); return; }
  d.health -= hit;
  refreshMap();
  if (distance(a, d) != 1 || unitReach(a.kind) > 1 || unitReach(d.kind) > 1) return;
  hit = damage(d, a);
  if (!hit) return;
  hit = rollDamage(hit);
  flash(2);
  if (hit >= a.health) { destroyUnit(selSide, selUnit); return; }
  a.health -= hit;
  refreshMap();
}

// The next target from enemy unit `from` in direction dir (1 or -1), round the list.
static uint8_t nextTarget(uint8_t from, int8_t dir) {
  uint8_t n = match.unitCount[selSide ^ 1];
  for (uint8_t k = 0; k < n; k++) {
    from = (from + n + dir) % n;
    if (isTarget(from)) return from;
  }
  return from;
}

// The targets show like the move range, the camera glides to the chosen one. The D-pad steps through
// them (right and down forward, left and up back), B attacks, A goes back to the menu (returns false).
static bool chooseTarget() {
  uint8_t t = nextTarget(match.unitCount[selSide ^ 1] - 1, 1);
  rangeOn = cursorShown = true;
  prepareCursor();
  while (true) {
    drawMap();
    if (!sendLayer()) continue;
    AWUnit &e = match.units[selSide ^ 1][t];
    glideTo(e.c, e.r);
    if (arduboy.justPressed(B_BUTTON)) { fight(t); return true; }
    if (arduboy.justPressed(A_BUTTON)) {
      AWUnit &u = match.units[selSide][selUnit];
      centerOn(u.c, u.r);
      updateMapView();
      return false;
    }
    if (arduboy.justPressed(RIGHT_BUTTON | DOWN_BUTTON)) t = nextTarget(t, 1);
    if (arduboy.justPressed(LEFT_BUTTON | UP_BUTTON)) t = nextTarget(t, -1);
  }
}

// Infantry on a building that is not its side's: the building loses the unit's health in capture
// points. At 0 it changes owner, the HQ ends the match. The unit hops on its tile meanwhile.
static void capture(uint8_t b) {
  static const uint8_t hop[12] PROGMEM = { 0, 2, 4, 5, 6, 6, 5, 4, 2, 0, 0, 0 };
  AWUnit &u = match.units[selSide][selUnit];
  u.moved = 1;
  walking = true;
  walkC = u.c;
  walkR = u.r;
  for (uint8_t f = 0; f < 36;) {
    drawMap();
    if (!sendLayer()) continue;
    walkX = tileX(u.c, u.r) + 12;
    walkY = tileY(u.c, u.r) + 10 - pgm_read_byte(&hop[f++ % 12]);
    mapChanged = f == 1;
    updateMapView();
  }
  walking = false;
  mapChanged = true;
  AWBuilding &bd = match.buildings[b];
  if (u.health < bd.capture) { bd.capture -= u.health; return; }
  readBuilding(b);
  bd.owner = selSide + 1;
  bd.capture = fullCapture(bldKind);
  if (!bldKind) { winner = selSide + 1; return; }
  computeFog();
  updateMapView();
  dialog(LOCA_captured);
}

// After the walk (or staying): ATTACK when a target is in reach, CAPTURE for infantry on a building
// of another owner, WAIT (FORTIFY for a land unit that stayed and has no sandbags yet), CANCEL.
// A works as CANCEL. After an ambush: no CAPTURE, no CANCEL. No CANCEL either when the walk
// uncovered an enemy.
static void unitMenu(bool ambushed, uint8_t seenBefore) {
  AWUnit &u = match.units[selSide][selUnit];
  bool canFortify = u.c == startC && u.r == startR && u.kind < 10 && !u.fortified;
  uint8_t b = buildingAt(u.c, u.r);
  bool canCapture = !ambushed && u.kind < 3 && b != 255 && match.buildings[b].owner != selSide + 1;
  bool canCancel = !ambushed && seenEnemies() <= seenBefore;
  while (true) {
    rangeOn = arrowOn = cursorShown = false;
    prepareCursor();
    uint24_t options[4];
    uint8_t n = 0;
    bool canAttack = markTargets();
    if (canAttack) options[n++] = LOCA_attack;
    if (canCapture) options[n++] = LOCA_capture;
    options[n++] = canFortify ? LOCA_fortify : LOCA_wait;
    if (canCancel) options[n++] = LOCA_cancel;
    int8_t chosen = showBox(options, n, false);
    if (chosen < 0) {
      if (!canCancel) continue;
      chosen = n - 1;
    }
    if (canAttack && chosen == 0) {
      if (chooseTarget()) break;
      continue;
    }
    if (canCapture && chosen == canAttack) {
      capture(b);
      break;
    }
    if (canCancel && chosen == n - 1) {
      // Cancel: back to where it stood, still selected.
      u.c = startC;
      u.r = startR;
      u.fortified = startFortified;
      centerOn(startC, startR);
      computeFog();
      updateMapView();
      floodRange();   // the targets took the range bits
      cursorShown = true;
      prepareCursor();
      return;
    }
    u.moved = 1;
    u.fortified |= canFortify;
    break;
  }
  computeFog();
  dropSelection();
}

// The shop of an own workshop, airport or shipyard with nobody on it, as in Arduwarsx 1.1: the units it
// sells with a cursor, the chosen unit's picture, price, attack and speed. B buys (the unit waits until
// the next turn), A closes. An X marks what the player can not buy.
static void shop(uint8_t kind) {
  uint8_t first = kind == 2 ? 0 : kind == 3 ? 10 : 13, count = kind == 2 ? 10 : kind == 3 ? 3 : 2;
  uint8_t side = match.side;
  int8_t chosen = 0;
  while (true) {
    drawScene();
    fillBox(3, 9, WIDTH - 6, HEIGHT - 10, BLACK);
    fillBox(4, 10, WIDTH - 8, HEIGHT - 13, WHITE);
    int8_t top = min((4 - chosen) * 8, 0);   // the list scrolls so the cursor stays on screen
    // the names follow each other on the FX chip: one read for the whole list
    FX::seekData(kind == 2 ? LOCA_unitNames : kind == 3 ? LOCA_planeNames : LOCA_shipNames);
    for (uint8_t i = 0; i < count; i++) {
      int8_t y = 14 + i * 8 + top;
      bool shown = y >= 10 && y <= HEIGHT - 8;
      uint8_t k = first + i;
      bool canBuy = pgm_read_byte(&unitPrice[k]) <= match.money[side] && match.unitCount[side] < MaxUnits;
      printX = 12 + (chosen == i && canBuy ? 4 : 0);
      printY = y;
      char c;
      while ((c = FX::readPendingUInt8()) != 0)
        if (shown) printChar(c);
      if (shown && !canBuy) { printX = 66; printChar('X'); }   // the player can not buy it
    }
    FX::readEnd();
    printX = 6;
    printY = 14 + chosen * 8 + top;
    printChar('>');
    uint8_t k = first + chosen;
    drawFX(88, 12, 24, 24, side ? unitsB : unitsA, k, arduboy.currentPlane());
    printAt(76, 38, LOCA_money);
    printNumber(90, 38, pgm_read_byte(&unitPrice[k]) * 100);
    printAt(76, 45, LOCA_attackShort);
    printNumber(115, 45, unitStat(k, 0));
    printAt(76, 52, LOCA_speed);
    printNumber(115, 52, pgm_read_byte(&unitMove[k]));
    if (!sendLayer()) continue;
    if (arduboy.justPressed(A_BUTTON)) return;
    if (arduboy.justPressed(DOWN_BUTTON) && chosen < count - 1) chosen++;
    if (arduboy.justPressed(UP_BUTTON) && chosen > 0) chosen--;
    if (!arduboy.justPressed(B_BUTTON)) continue;
    uint8_t price = pgm_read_byte(&unitPrice[k]);
    if (price > match.money[side] || match.unitCount[side] == MaxUnits) continue;
    match.money[side] -= price;
    AWUnit &u = match.units[side][match.unitCount[side]++];
    u = AWUnit();
    u.c = cursorC;
    u.r = cursorR;
    u.kind = k;
    u.health = FullHealth;
    u.moved = 1;
    computeFog();
    return;
  }
}

#ifdef AW_MEASURE
// Measuring build: a test army on map 0 and fixed test views, so tools/ardens/speedtest.py measures the
// same pictures in every build. A on the free cursor shows the next view.
// 16 units of player 1 packed by the forest (some hurt), 4 enemy tanks next to them.
static void testArmy() {
  for (uint8_t i = 0; i < 16; i++) {
    AWUnit &u = match.units[0][i];
    u.c = 6 + i % 4; u.r = 5 + i / 4; u.kind = i % 10; u.health = FullHealth - i % 3;
  }
  for (uint8_t i = 0; i < 4; i++) {
    AWUnit &u = match.units[1][i];
    u.c = 10 + i % 2; u.r = 6 + i / 2; u.kind = 5; u.health = FullHealth;
  }
  match.unitCount[0] = 16;
  match.unitCount[1] = 4;
}

// Test views: tile under the cursor, then 0 plain, 1 unit 5 selected, 2 unit 0 selected with a
// zigzag arrow of 9 steps.
const uint8_t testViews[][3] PROGMEM = {
  {7, 6, 0}, {7, 6, 1}, {8, 4, 2}, {13, 8, 0}, {8, 13, 0}, {2, 9, 0}, {5, 6, 0}, {6, 4, 0},
};
static void nextTestView() {
  static uint8_t view;
  uint8_t mode = pgm_read_byte(&testViews[view][2]);
  turn = AWTurn::Free;
  rangeOn = arrowOn = false;
  cursorShown = true;
  if (mode) {
    selSide = 0;
    selUnit = mode == 1 ? 5 : 0;
    floodRange();
    turn = AWTurn::Selected;
  }
  if (mode == 2) {
    for (uint8_t i = 0; i < MaxPath; i++) pathDir[i] = i & 1 ? 3 : 0;
    pathLength = MaxPath;
    arrowOn = true;
    cursorShown = false;
  }
  centerOn(pgm_read_byte(&testViews[view][0]), pgm_read_byte(&testViews[view][1]));
  refreshMap();
  view = (view + 1) % (sizeof(testViews) / 3);
}
#endif

// A new match on map 0, as in Arduwars 1.1: no units, 1000 money, the start buildings from the map.
static void startMatch() {
  memset(&match, 0, sizeof(match));
  loadMap(0);
  match.day = 1;
  match.money[0] = match.money[1] = 10;
#ifdef AW_MEASURE
  testArmy();
#endif
  winner = 0;
  turn = AWTurn::Free;
  startTurn();
}

// The end of the match: who won, then a new match. The results screen comes in step 3.
static void endMatch() {
  rangeOn = arrowOn = false;
  cursorShown = true;
  dialog(winner == 1 ? LOCA_p1Wins : winner == 2 ? LOCA_p2Wins : LOCA_draw);
  wipe(false);
  startMatch();
}

static void endTurnMenu() {
  const uint24_t options[3] = { LOCA_unitsLeft, LOCA_endTurn, LOCA_cancel };
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
  if (winner) { endMatch(); return; }
  uint8_t lastC = cursorC, lastR = cursorR;
  panCamera();
  updateMapView();
  if (turn == AWTurn::Selected && (cursorC != lastC || cursorR != lastR)) {
    buildPath(cursorC, cursorR);
    cursorShown = !arrowOn;
    prepareCursor();
  }
#ifdef AW_MEASURE
  if (arduboy.justPressed(A_BUTTON)) { nextTestView(); return; }
#endif
  if (arduboy.justPressed(A_BUTTON) && turn == AWTurn::Selected) dropSelection();
  if (!arduboy.justPressed(B_BUTTON)) return;
  if (turn == AWTurn::Selected) {
    if (!cursorOn || !canStop(cursorC, cursorR)) return;
    uint8_t seen = seenEnemies();
    bool ambushed = arrowOn && cutAtAmbush();
    if (arrowOn && pathLength) walkPath();
    computeFog();
    updateMapView();
    if (ambushed) dialog(LOCA_ambush);
    unitMenu(ambushed, seen);
    return;
  }
  if (cursorOn) {
    uint8_t i = unitAt(match.side, cursorC, cursorR);
    if (i != 255 && !match.units[match.side][i].moved) {
      selSide = match.side;
      selUnit = i;
      startFortified = match.units[selSide][i].fortified;
      floodRange();
      turn = AWTurn::Selected;
      arrowOn = false;
      return;
    }
    if (!unitOn(cursorC, cursorR)) {
      uint8_t b = buildingAt(cursorC, cursorR);
      if (b != 255 && match.buildings[b].owner == match.side + 1 && bldKind >= 2) {
        shopOpen = true;
        shop(bldKind);
        shopOpen = false;
        return;
      }
    }
  }
  endTurnMenu();
}
