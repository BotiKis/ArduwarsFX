#pragma once
#include "fxdata/titleLayers.h"
// The title, in two screens. B confirms, A goes back.
// - Start screen: the picture in layers that glide with the D-pad, the logo and a white PRESS B.
// - Menu screen: the battlefield one shade darker, the pinup slides in on the left, a heading top right,
//   a list of slanted plates under it that scrolls, the chosen plate slides out. 1 PLAYER waits for the AI (step 4),
//   2 PLAYER opens the map list, SOUND switches the sound and keeps it, CREDITS lists the names.
// Then the match (loop in ArduwarsFX.ino), then the results box over the last map view.

constexpr uint8_t MenuX = 54;         // left edge of the plates, they run off the right edge
constexpr uint8_t MenuY = 15;         // top of the first plate
constexpr uint8_t MenuStep = 12;      // plates apart
constexpr uint8_t MenuRows = 4;       // rows on screen, the list scrolls through more
constexpr int8_t ChosenOut = 6;       // the chosen plate slides this far out to the left
constexpr uint8_t CreditRows = 18;    // lines from LOCA_creditSheet on
constexpr uint8_t PinupW = 56;        // Assets/Sprites/pinup_56x64.png

// The list's state (`list`) shares its bytes with the top bar's letters, see Turn.h.

static void drawMenu() {
  uint8_t plane = arduboy.currentPlane();
  SpritesU::drawBasic(0, 0, WIDTH, HEIGHT, menuBack + 4, plane, SpritesU::MODE_OVERWRITEFX);   // no mask
  drawFX(48, 1, 80, 16, headings, list.heading, plane);
  drawFX(list.pinupX, 0, PinupW, 64, pinup, 0, plane);   // behind the plates
  // A plate per line (Assets/Sprites/plates_80x16.png): dark with white letters, the chosen one white
  // with black letters, slid out to the left and 1 pixel up. BACK plates carry a left chevron.
  uint24_t next = list.first;
  for (uint8_t i = 0; i < list.count && i < list.top + MenuRows; i++) {
    uint24_t text = next;
    next += textLength(text) + 1;
    if (i < list.top) continue;
    if (text == LOCA_soundOn && !arduboy.audio.enabled()) text = LOCA_soundOff;
    bool chosen = i == list.chosen;
    uint8_t x = MenuX - (chosen ? list.slide : 0);
    uint8_t y = MenuY + MenuStep * (i - list.top) - (chosen && list.slide > 2);
    bool back = list.heading && i == 0;   // the BACK line on top of the maps and credits: with a chevron
    drawFX(x, y, 80, 16, plates, chosen + 2 * back, plane);
    textWhite = !chosen;
    printAt(x + 7 + 4 * back, y + 4, text);
  }
  textWhite = false;
}

// The credits: the BACK plate (B or A goes back) and a white sheet under it, a name in black and its role
// under it in dark gray, dotted lines between people. Up and down scroll a line. See LOCA_creditSheet.
static void credits() {
  list.first = LOCA_creditsBack;   // the menu screen with one line: BACK
  list.count = 1;
  list.heading = 2;
  list.chosen = list.top = list.slide = 0;
  uint8_t top = 0, total = 0;   // the sheet's row at its top, all its rows
  while (true) {
    drawMenu();
    uint8_t plane = arduboy.currentPlane();
    SpritesU::fillRect(50, 28, 76, 34, 0);   // 2 pixels short of the screen's right and bottom edges
    SpritesU::fillRect(51, 29, 74, 32, 1);
    // Every line gets its row on the whole sheet (pos), shown from row 31 on, scrolled by `top`. Only whole
    // lines show. A name has 4 free rows under it, a role line 1. Between people: 8 free rows, the dots, 6 free rows.
    uint24_t text = LOCA_creditSheet;
    uint8_t pos = 0;
    for (uint8_t i = 0; i < CreditRows; i++) {
      uint24_t line = text;
      text += textLength(line) + 1;
      FX::seekData(line);
      bool role = FX::readPendingUInt8() == '~';
      FX::readEnd();
      if (!role && i) {
        int16_t dots = 31 + pos + 7 - top;   // light gray dots: dark in the dim layer
        if (plane == 0 && dots >= 30 && dots <= 59)
          for (uint8_t x = 53; x < 121; x += 2) arduboy.getBuffer()[(dots >> 3) * WIDTH + x] &= ~(1 << (dots & 7));
        pos += 14;
      }
      int16_t y = 31 + pos - top;
      pos += role ? 5 : 8;
      if (y < 30 || y > 56) continue;
      if (!role) printAt(53, y, line);
      else if (plane) printAt(53, y, line + 1);   // dark gray: black only in the bright layer
    }
    total = pos;
    uint8_t range = total - 30;
    drawScrollBar(123, 29, 32, top, range);   // the sheet's full height
    if (!sendLayer()) continue;
    if (arduboy.justPressed(B_BUTTON)) { playSound(SoundConfirm); return; }
    if (arduboy.justPressed(A_BUTTON)) { playSound(SoundBack); return; }
    list.slide += (ChosenOut - list.slide + 1) / 2;
    int8_t step = scrollStep();   // 5 rows a step
    if (step > 0) top = top + 5 < range ? top + 5 : range;
    if (step < 0) top = top > 5 ? top - 5 : 0;
  }
}

// `count` texts from `first` on as a list on the menu screen. Returns the chosen line on B, -1 on A.
static int8_t menu(uint8_t heading, uint24_t first, uint8_t count, int8_t chosen) {
  scene = AWScene::Menu;
  list.first = first;
  list.count = count;
  list.heading = heading;
  list.chosen = chosen;
  list.top = 0;
  list.slide = 0;
  while (true) {
    if (list.chosen < list.top) list.top = list.chosen;
    if (list.chosen >= list.top + MenuRows) list.top = list.chosen - MenuRows + 1;
    drawMenu();
    if (!sendLayer()) continue;
    list.pinupX = list.pinupX * 3 / 4;   // slides in, slower at the end
    if (arduboy.justPressed(B_BUTTON)) { playSound(SoundConfirm); return list.chosen; }
    if (arduboy.justPressed(A_BUTTON)) { playSound(SoundBack); return -1; }
    list.slide += (ChosenOut - list.slide + 1) / 2;   // the chosen plate slides out, slower at the end
    if (arduboy.justPressed(DOWN_BUTTON)) { list.chosen = list.chosen + 1 == list.count ? 0 : list.chosen + 1; list.slide = 0; }
    if (arduboy.justPressed(UP_BUTTON)) { list.chosen = list.chosen ? list.chosen - 1 : list.count - 1; list.slide = 0; }
  }
}

// The start screen until B. The picture is made of layers, back to front: sky, ground, dust, dust behind the
// troops, troops, tank, dust in front and the logo, which stands still (Assets/Sprites/title-NN-*-160x64.png, see tools/titlelayers.py). Each is a see-through picture
// of the 160x64 scene, the screen shows the middle 128 columns. Hold the D-pad and the layers glide that way,
// each its own distance (titleLean, less up and down), let go and they glide back. The two dust layers
// around the troops also drift on their own.
// A blinking white PRESS B with a black edge sits in the middle at the bottom.
static const int8_t titleLean[] PROGMEM = {-6, 3, 4, 7, 10, 16, 16, 0};   // pixels at full lean, minus: against the pad
static const int8_t titleLeanY[] PROGMEM = {-2, 2, 1, 2, 3, 5, 5, 0};      // the same up and down
// Drifting layers: pictures per pixel, 0 stands still. They repeat every 160 columns.
static const uint8_t titleDrift[] PROGMEM = {0, 0, 0, 8, 0, 0, 2, 0};
constexpr uint8_t LogoLayer = 7;     // waits 0.6 s above the screen, then drops in and bounces once
constexpr int8_t FullLean = 64;

// Moves a lean toward its target, slower at the end.
static void glide(int8_t &lean, int8_t target) {
  int8_t d = (target - lean) / 8;   // counted in int: from -64 to 64 does not fit an int8_t (the old jump)
  lean += d ? d : (target > lean) - (target < lean);
}

// A layer kept as a list of its pixels (see tools/titlelayers.py), moved by dx, dy on the screen.
// It repeats every 160 columns.
AW_FAST static void drawPixels(uint24_t at, int16_t dx, int8_t dy, uint8_t plane) {
  dx += 16;   // the shift in scene columns, wrapped into 0-159
  while (dx < 0) dx += 160;
  FX::seekData(at);
  while (true) {
    uint8_t x = FX::readPendingUInt8(), yShade = FX::readPendingUInt8();
    if (yShade == 0xFF) break;
    int16_t sx = x + dx;
    if (sx >= 160) sx -= 160;
    sx -= 16;
    int8_t y = (yShade >> 2) + dy;
    if (sx < 0 || sx >= WIDTH || y < 0 || y >= HEIGHT) continue;
    uint8_t *b = arduboy.getBuffer() + (y >> 3) * WIDTH + sx, bit = 1 << (y & 7);
    if (yShade >> plane & 1) *b |= bit;
    else *b &= ~bit;
  }
  FX::readEnd();
}

static void startScreen() {
  constexpr uint8_t pressX = (WIDTH - 7 * 5 + 1) / 2;   // PRESS B is 7 letters
  int8_t lean = 0, leanY = 0;   // -64 to 64
  int8_t logoY = -24, logoSpeed = 0;
  uint8_t logoWait = 30;        // pictures
  uint16_t drift = 0;   // counts pictures, 0 to 1279 (160 times 8 and 2)
  // PRESS B's letters, printed once: the bottom 16 rows of each column, from 2 columns left of the text on.
  // Printing the black edge as 8 more copies took 1.4 ms per layer.
  uint16_t letters[39];
  memset(arduboy.getBuffer(), 0, WIDTH * HEIGHT / 8);
  textWhite = true;
  printAt(pressX, 56, LOCA_pressB);
  textWhite = false;
  uint8_t *bottom = arduboy.getBuffer() + 6 * WIDTH + pressX - 2;
  for (uint8_t i = 0; i < 39; i++) letters[i] = bottom[i] | bottom[i + WIDTH] << 8;
  while (true) {
    uint8_t plane = arduboy.currentPlane();
    for (uint8_t i = 0; i < TitleLayers; i++) {
      AWTitleLayer t;
      memcpy_P(&t, titleTable + i, sizeof t);
      int16_t x = t.x - 16 + lean * int8_t(pgm_read_byte(titleLean + i)) / FullLean;
      int8_t y = t.y + leanY * int8_t(pgm_read_byte(titleLeanY + i)) / FullLean;
      if (i == LogoLayer) y += logoY;
      if (y > 0 && i == 0) SpritesU::fillRect(0, 0, WIDTH, y, plane);   // light gray over the sky when it moves down
      if (i == 1 && y < t.y) SpritesU::fillRect(0, HEIGHT - t.y + y, WIDTH, t.y - y, plane);   // light gray under the ground
      uint8_t slow = pgm_read_byte(titleDrift + i);
      if (slow) x -= drift / slow % 160;
      if (t.kind == 2) { drawPixels(titleLayers + t.start, x - t.x, y - t.y, plane); continue; }
      uint8_t mode = t.kind ? SpritesU::MODE_PLUSMASKFX : SpritesU::MODE_OVERWRITEFX;
      if (slow) {
        SpritesU::drawBasic(x + 160, y, t.w, t.h, titleLayers + t.start, plane, mode);
      }
      SpritesU::drawBasic(x, y, t.w, t.h, titleLayers + t.start, plane, mode);
    }
    if (pictureCount & 32) for (uint8_t i = 1; i < 38; i++) {   // blinks: the letters in white, the pixels around them black
      uint16_t near = letters[i - 1] | letters[i] | letters[i + 1];
      uint16_t pixels = (bottom[i] | bottom[i + WIDTH] << 8) & ~(near | near << 1 | near >> 1) | letters[i];
      bottom[i] = pixels;
      bottom[i + WIDTH] = pixels >> 8;
    }
    if (!sendLayer()) continue;
    glide(lean, arduboy.pressed(RIGHT_BUTTON) ? FullLean : arduboy.pressed(LEFT_BUTTON) ? -FullLean : 0);
    glide(leanY, arduboy.pressed(DOWN_BUTTON) ? FullLean : arduboy.pressed(UP_BUTTON) ? -FullLean : 0);
    if (++drift == 1280) drift = 0;
    if (logoWait) logoWait--;
    else if (logoY < 0 || logoSpeed) {   // falls faster each picture, bounces back at half speed
      logoY += ++logoSpeed;
      if (logoY >= 0) {
        logoY = 0;
        logoSpeed = logoSpeed < 6 ? 0 : -logoSpeed / 2;
      }
    }
    if (arduboy.justPressed(B_BUTTON)) { playSound(SoundConfirm); return; }
  }
}

// The title screens. Returns the map picked for a 2-player match.
static uint8_t titleScreens() {
  playMenuTheme();
  while (true) {
    startScreen();
    list.pinupX = -PinupW;
    int8_t chosen = 0;
    while (true) {
      chosen = menu(0, LOCA_mainMenu, 4, chosen);
      if (chosen < 0) break;   // back to the start screen
      if (chosen == 0) dialog(LOCA_comingSoon);   // the AI comes in step 4
      if (chosen == 2) { arduboy.audio.toggle(); arduboy.audio.saveOnOff(); }
      if (chosen == 3) credits();
      if (chosen != 1) continue;
      FX::seekData(mapList);
      uint8_t maps = FX::readPendingUInt8();
      FX::readEnd();
      int8_t map = menu(1, LOCA_mapsMenu, maps + 1, 1) - 1;   // BACK, then the maps: starts on the first map
      if (map < 0) continue;
      wipe(false);
      return map;
    }
  }
}

// The end of a match: a box over the last map view, the result in its tab, a line per number with a
// column per player (days only once). Up and down scroll the lines (the game's scroll bar), B goes back to the title.
constexpr uint8_t ResultRows = 8, ResultsShown = 5;   // ResultRows: the lines from LOCA_resultRows on

// Money in hundreds as the top bar shows it (40 shows as 4000), from 1000 on in thousands (1234 as 123K).
static void printMoney(uint8_t x, uint8_t y, uint16_t hundreds) {
  printNumber(x, y, hundreds >= 1000 ? hundreds / 10 : hundreds);
  if (hundreds >= 1000) printChar('K');
  else if (hundreds) { printChar('0'); printChar('0'); }
}

static void results() {
  playEndTune(winner < 3);
  rangeOn = arrowOn = false;
  cursorShown = true;
  prepareCursor();
  uint24_t head = winner == 1 ? LOCA_p1Wins : winner == 2 ? LOCA_p2Wins : LOCA_draw;
  uint8_t held[2] = {};
  for (uint8_t i = 0; i < buildingCount; i++)
    if (match.buildings[i].owner) held[match.buildings[i].owner - 1]++;
  uint8_t scroll = 0;
  while (true) {
    drawMap();
    uint8_t top = drawBox(2, 9, WIDTH - 4, HEIGHT - 11, head);   // the same box as the shop: the panel starts at row 14
    printAt(66, top + 5, LOCA_p1);
    printAt(94, top + 5, LOCA_p2);
    drawScrollBar(119, top + 11, 7 * ResultsShown - 1, scroll, ResultRows - ResultsShown);   // next to the lines
    // the line names follow each other on the FX chip: one read for all
    FX::seekData(LOCA_resultRows);
    for (uint8_t r = 0; r < ResultRows; r++) {
      bool shown = r >= scroll && r < scroll + ResultsShown;
      uint8_t y = top + 12 + 7 * (r - scroll);
      printX = 13;
      printY = y;
      char c;
      while ((c = FX::readPendingUInt8()) != 0)
        if (shown) printChar(c);
      if (!shown) continue;
      for (uint8_t side = 0; side < 2; side++) {
        uint8_t x = 66 + 28 * side;
        switch (r) {
          case 0: if (!side) printNumber(x, y, match.day); break;
          case 1: printNumber(x, y, match.destroyed[side]); break;
          case 2: printNumber(x, y, match.unitCount[side]); break;
          case 3: printNumber(x, y, match.bought[side]); break;
          case 4: printNumber(x, y, held[side]); break;
          case 5: printNumber(x, y, match.captured[side]); break;
          case 6: printMoney(x, y, match.money[side]); break;
          default: printMoney(x, y, match.spent[side]);
        }
      }
    }
    FX::readEnd();
    if (!sendLayer()) continue;
    if (arduboy.justPressed(B_BUTTON)) { playSound(SoundConfirm); break; }
    int8_t step = scrollStep();
    if (step < 0 && scroll) scroll--;
    if (step > 0 && scroll < ResultRows - ResultsShown) scroll++;
  }
  wipe(false);
}
