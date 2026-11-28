#pragma once
// The isometric map view: camera, cursor and drawing.
//
// Projection: tile (c, r) has its 48x48 picture at x = (c-r)*24 + originX, y = (c+r)*12 on the map picture.
// Its ground diamond: top corner at (24, 16), center (24, 28), bottom corner (24, 40) in the picture.
// The terrain and the buildings of a map are baked into one picture on the FX chip (tools/mapconv.py),
// cut into 64x64 blocks. A layer draws the 2 to 6 blocks on screen, then the draw list:
// cursor, units, owner marks, and the tops of trees, mountains and buildings that stand in front
// of a unit or the cursor. The draw list is made ahead for a margin around the screen, so panning
// rarely has to make it again, and the 3 layers read no map data.

// The draw list is made in the time of a layer: compiled for speed (about 350 bytes more flash).
// Without it, moving the cursor through a crowd made a third of the layers late.
#define AW_FAST __attribute__((optimize("O2")))

// Map record on the FX chip, see the top of fxdata/maps.txt.
constexpr uint8_t MapHeaderSize = 20;

uint24_t mapGrid;         // FX address: one byte per tile
uint24_t mapFirstBuilding;  // FX address: per diagonal c+r its first building
uint24_t mapBuildings;    // FX address: 7 bytes per building, back to front
uint8_t tallBits[MaxMapSize * MaxMapSize / 8];   // tiles with a tree or mountain top, bit r*32 + c
uint24_t mapBlockTable;   // FX address: one uint16 per 64x64 block
uint8_t mapW, mapH, blockCols, buildingCount;
int16_t originX;

int16_t camX, camY;       // map picture pixel at the top left of the screen
uint8_t cursorC, cursorR;   // the tile under the screen center
bool cursorOn;              // false when the screen center is off the map
bool mapChanged;          // set when units or owners change: the draw list is made again
uint8_t pictureCount;     // counts pictures, for the marching outline of the range
bool cursorShown = true;  // false while the arrow, a walk or a box shows

// The selected unit on its walk: drawn at a pixel position instead of its tile, in the cursor's part
// of the draw list.
bool walking;
uint8_t walkC, walkR;                       // the tile it walks into
int16_t walkX, walkY;                       // its picture on the map picture

// A fight: the cursor shows on both tiles while fightOn. A unit hidden while it flickers out.
bool fightOn;
uint8_t fightC[2], fightR[2];
uint8_t hideSide = 255, hideUnit;

// In Turn.h: the letters, for the health number, and the top bar over the map.
extern int16_t printX, printY;
static void printChar(char c);
static void drawTopBar();
static void prepareTopBar();
AW_FAST static void drawBadge(int16_t x, int16_t y, uint8_t health);
constexpr uint8_t CenterY = (HEIGHT + 7) / 2;   // middle of the map below the top bar

// In Rules.h: the selected unit, and the move range and the movement arrow, drawn under the units.
extern uint8_t selSide, selUnit;
extern bool rangeOn, arrowOn;
AW_FAST static void drawTiles(uint8_t plane, uint8_t phase, bool fog);
static void drawArrow(uint8_t plane);

// The 2 to 6 blocks on screen: block index in mapBlocks.
uint16_t blocks[2][3];
uint8_t blockC, blockR;   // first block column and row on screen

// DarkTop: a top in fog. Bar: capture points left of a building, frame = points.
enum class AWPic : uint8_t { Top, DarkTop, Cursor, Mark, Bar, UnitA, UnitB, UnitADone, UnitBDone };
constexpr uint8_t PicFortified = 0x80;   // added to a unit's pic: sandbags at its feet
struct AWDrawItem {
  uint8_t key;            // draw order: diagonal c+r times 4, then top 0, mark 1, cursor 2, unit 3
  AWPic pic;
  uint8_t frame;          // units: kind + 16 * health
  int16_t x, y;           // on the map picture
};
// The draw list: first the units' part (units, owner marks, tops in front of units), made for the screen
// plus ListMargin around it, then the cursor's part (cursor and the tops in front of it), made per picture.
constexpr int16_t ListMargin = 32;
constexpr uint8_t MaxListItems = 48;
constexpr uint8_t MaxDrawItems = MaxListItems + 4;
AWDrawItem drawItems[MaxDrawItems];
uint8_t drawOrder[MaxDrawItems];   // item numbers sorted by key
uint8_t drawCount, listCount, drawLimit;
int16_t winX0, winY0;              // top left of the map picture area the list covers
constexpr int16_t ListW = WIDTH + 2 * ListMargin, ListH = HEIGHT + 2 * ListMargin;   // its size

// Buildings in that area, for the tops in front of the cursor.
struct AWNearBuilding { uint8_t c, r, kind; int16_t x, y; };
constexpr uint8_t MaxNear = 8;
AWNearBuilding nearBuildings[MaxNear];
uint8_t nearCount;

// Big-endian reads from the FX chip, one byte at a time. ponytail: FX::readPendingUInt16/24 gave wrong
// values in this build (their inline assembly depends on where the compiler puts them).
static uint16_t fxRead16() { uint16_t v = FX::readPendingUInt8() << 8; return v | FX::readPendingUInt8(); }
static uint24_t fxRead24() { uint24_t v = uint24_t(fxRead16()) << 8; return v | FX::readPendingUInt8(); }

#include "Fog.h"

static int16_t tileX(uint8_t c, uint8_t r) { return (int16_t(c) - r) * 24 + originX; }
static int16_t tileY(uint8_t c, uint8_t r) { return (c + r) * 12; }

static void centerOn(uint8_t c, uint8_t r) {
  camX = tileX(c, r) + 24 - WIDTH / 2;
  camY = tileY(c, r) + 28 - CenterY;
}

static void findCursor() {
  // Screen center to tile: undo the projection, relative to the top corner of tile (0, 0).
  int16_t dx = camX + WIDTH / 2 - (originX + 24);
  int16_t dy = camY + CenterY - 16;
  int16_t a = 2 * dy + dx, b = 2 * dy - dx;
  cursorOn = a >= 0 && b >= 0 && a / 48 < mapW && b / 48 < mapH;
  cursorC = a / 48;
  cursorR = b / 48;
}

static void loadMap(uint8_t index) {
  FX::seekData(mapList + 1 + index * 3);
  uint24_t map = fxRead24();
  FX::readEnd();
  FX::seekData(map);
  mapW = FX::readPendingUInt8();
  mapH = FX::readPendingUInt8();
  blockCols = FX::readPendingUInt8();
  FX::readPendingUInt8();   // block rows
  buildingCount = FX::readPendingUInt8();
  mapBlockTable = fxRead24();
  FX::readEnd();
  mapGrid = map + MapHeaderSize;
  mapFirstBuilding = mapGrid + mapW * mapH;
  mapBuildings = mapFirstBuilding + mapW + mapH - 1;
  memset(tallBits, 0, sizeof(tallBits));
  FX::seekData(mapGrid);
  for (uint8_t r = 0; r < mapH; r++)
    for (uint8_t c = 0; c < mapW; c++)
      if (FX::readPendingUInt8() >> 4) tallBits[(r * 32 + c) / 8] |= 1 << (c & 7);
  FX::readEnd();
  originX = (mapH - 1) * 24;
  mapChanged = true;

  // Start owners, and each side's camera on its HQ.
  match.map = index;
  FX::seekData(mapBuildings);
  for (uint8_t i = 0; i < buildingCount; i++) {
    uint8_t c = FX::readPendingUInt8();
    uint8_t r = FX::readPendingUInt8();
    uint8_t kindOwner = FX::readPendingUInt8();
    fxRead16();
    fxRead16();
    match.buildings[i].owner = kindOwner >> 4;
    match.buildings[i].capture = fullCapture(kindOwner & 15);
    if ((kindOwner & 15) == 0 && kindOwner >> 4) {   // an HQ: its side's first view
      centerOn(c, r);
      match.viewX[(kindOwner >> 4) - 1] = camX;
      match.viewY[(kindOwner >> 4) - 1] = camY;
    }
  }
  FX::readEnd();
}

AW_FAST static void addDrawItem(uint8_t key, AWPic pic, uint8_t frame, int16_t x, int16_t y) {
  if (x <= winX0 - 48 || x >= winX0 + ListW || y <= winY0 - 48 || y >= winY0 + ListH) return;
  // ponytail: a full list drops the rest. 48 items fit a crowded screen and its margin.
  if (drawCount == drawLimit) return;
  drawItems[drawCount] = { key, pic, frame, x, y };
  uint8_t i = drawCount;
  for (; i > 0 && drawItems[drawOrder[i - 1]].key > key; i--) drawOrder[i] = drawOrder[i - 1];
  drawOrder[i] = drawCount++;
}

static void addTop(uint8_t key, AWPic pic, uint8_t frame, int16_t x, int16_t y) {
  for (uint8_t i = 0; i < drawCount; i++) {
    AWDrawItem &d = drawItems[i];
    if (d.pic == pic && d.key == key && d.x == x && d.frame == frame) return;   // already there
  }
  addDrawItem(key, pic, frame, x, y);
}

static AWPic topPic(uint8_t c, uint8_t r) { return isFog(c, r) ? AWPic::DarkTop : AWPic::Top; }

// A unit or the cursor stands right behind tile (c, r): draw the tile's tree or mountain top again.
AW_FAST static void addFront(uint8_t c, uint8_t r) {
  uint16_t bit = r * 32 + c;
  if (c >= mapW || r >= mapH || !(tallBits[bit / 8] & (1 << (bit & 7)))) return;
  FX::seekData(mapGrid + r * mapW + c);
  addTop((c + r) * 4, topPic(c, r), (FX::readEnd() >> 4) - 1, tileX(c, r), tileY(c, r));
}

// Tiles with a unit in the list area, for the building tops. Kept at the end of the screen buffer while
// prepareList runs between two layers, cleared again after.
constexpr uint8_t MaxStanding = 24;
static uint8_t *standing() { return arduboy.getBuffer() + WIDTH * HEIGHT / 8 - 2 * MaxStanding; }
uint8_t standCount;

static bool hasStanding(uint8_t c, uint8_t r) {
  uint8_t *stand = standing();
  for (uint8_t i = 0; i < standCount; i++)
    if (stand[i] == c && stand[MaxStanding + i] == r) return true;
  return false;
}

// The units' part of the draw list, when the camera left the area of the last one.
AW_FAST static void prepareList() {
  winX0 = camX - ListMargin;
  winY0 = camY - ListMargin;
  int16_t winX1 = winX0 + ListW, winY1 = winY0 + ListH;
  drawCount = standCount = nearCount = 0;
  drawLimit = MaxListItems;
  // Tiles in the area, a little generous: diagonal c+r and screen column c-r.
  int8_t sLo = (winY0 - 48) / 12, sHi = winY1 / 12;
  int8_t dLo = (winX0 - originX - 48) / 24 - 1, dHi = (winX1 - originX) / 24 + 1;
  for (uint8_t side = 0; side < 2; side++)
    for (uint8_t i = 0; i < match.unitCount[side]; i++) {
      AWUnit &u = match.units[side][i];
      if ((walking && side == selSide && i == selUnit) || (side == hideSide && i == hideUnit)) continue;
      int8_t c = u.c, r = u.r;
      if (c + r < sLo || c + r > sHi || c - r < dLo || c - r > dHi) continue;
      if (side != match.side && isFog(c, r)) continue;   // hidden in the fog
      // unit bottom 6 pixels below the diamond center
      uint8_t before = drawCount;
      addDrawItem((c + r) * 4 + 3, AWPic(uint8_t(AWPic::UnitA) + side + 2 * u.moved + (u.fortified ? PicFortified : 0)),
                  u.kind | u.health << 4, tileX(c, r) + 12, tileY(c, r) + 10);
      if (drawCount == before) continue;
      if (standCount < MaxStanding) { standing()[standCount] = c; standing()[MaxStanding + standCount++] = r; }
      addFront(c + 1, r);
      addFront(c, r + 1);
      addFront(c + 1, r + 1);
    }
  // Only the buildings on the diagonals near the area. A building picture may sit up to a tile off its tile.
  int16_t s = (winY0 - 48 - 24) / 12;
  FX::seekData(mapFirstBuilding + max(s, 0));
  uint8_t i = FX::readEnd();
  uint8_t sEnd = (winY1 + 24) / 12;
  FX::seekData(mapBuildings + i * 7);
  for (; i < buildingCount; i++) {
    uint8_t c = FX::readPendingUInt8();
    uint8_t r = FX::readPendingUInt8();
    if (c + r > sEnd) break;
    uint8_t kind = FX::readPendingUInt8() & 15;
    int16_t x = fxRead16();
    int16_t y = fxRead16();
    if (x <= winX0 - 48 || x >= winX1 || y <= winY0 - 48 || y >= winY1) continue;
    // ponytail: past 8 buildings in the area, the cursor gets no building tops. A screen shows about 6.
    if (nearCount < MaxNear) nearBuildings[nearCount++] = { c, r, kind, x, y };
    AWBuilding &b = match.buildings[i];
    bool fog = isFog(c, r);
    if (b.owner && !fog) addDrawItem((c + r) * 4 + 1, AWPic::Mark, b.owner - 1, x + 20, y + 34);   // at the front corner
    if (b.capture < fullCapture(kind) && !fog) addDrawItem((c + r) * 4 + 1, AWPic::Bar, b.capture, tileX(c, r), tileY(c, r));
    if (hasStanding(c - 1, r) || hasStanding(c, r - 1) || hasStanding(c - 1, r - 1) || behindIs(c, r, !fog))
      addDrawItem((c + r) * 4, fog ? AWPic::DarkTop : AWPic::Top, kind, x, y);
  }
  FX::readEnd();
  // Fog edge: a seen tree or mountain stays bright over the fog behind it, one in fog stays dark over seen tiles.
  for (int8_t s = max(sLo, 0); s <= min(sHi, int8_t(mapW + mapH - 2)); s++)
    for (int8_t d = dLo + ((dLo ^ s) & 1); d <= dHi; d += 2) {
      int8_t c = (s + d) >> 1, r = (s - d) >> 1;
      uint16_t bit = r * 32 + c;
      if (c < 0 || r < 0 || c >= int8_t(mapW) || r >= int8_t(mapH) || !(tallBits[bit / 8] & (1 << (bit & 7)))) continue;
      bool fog = isFog(c, r);
      if (!behindIs(c, r, !fog)) continue;
      FX::seekData(mapGrid + r * mapW + c);
      addTop((c + r) * 4, fog ? AWPic::DarkTop : AWPic::Top, (FX::readEnd() >> 4) - 1, tileX(c, r), tileY(c, r));
    }
  memset(standing(), 0, 2 * MaxStanding);
  listCount = drawCount;
}

// The tops of trees, mountains and buildings in front of tile (c, r).
static void addFronts(uint8_t c, uint8_t r) {
  addFront(c + 1, r);
  addFront(c, r + 1);
  addFront(c + 1, r + 1);
  for (uint8_t i = 0; i < nearCount; i++) {
    AWNearBuilding &b = nearBuildings[i];
    if ((b.c == c + 1 && b.r == r) || (b.c == c && b.r == r + 1) || (b.c == c + 1 && b.r == r + 1))
      addTop((b.c + b.r) * 4, topPic(b.c, b.r), b.kind, b.x, b.y);
  }
}

// The cursor's part of the draw list (cursor or walking unit), each picture the cursor changed tile,
// each picture of a walk, and after prepareList.
AW_FAST static void prepareCursor() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < drawCount; i++)
    if (drawOrder[i] < listCount) drawOrder[n++] = drawOrder[i];
  drawCount = listCount;
  drawLimit = MaxDrawItems;
  if (walking) {
    AWUnit &u = match.units[selSide][selUnit];
    addDrawItem((walkC + walkR) * 4 + 3, AWPic(uint8_t(AWPic::UnitA) + selSide), u.kind | u.health << 4, walkX, walkY);
    addFronts(walkC, walkR);
    addFronts(u.c, u.r);
  }
  for (uint8_t i = 0; fightOn && i < 2; i++) {
    addDrawItem((fightC[i] + fightR[i]) * 4 + 2, AWPic::Cursor, 0, tileX(fightC[i], fightR[i]), tileY(fightC[i], fightR[i]) + 16);
    addFronts(fightC[i], fightR[i]);
  }
  if (!cursorOn || !cursorShown) return;
  uint8_t c = cursorC, r = cursorR;
  addDrawItem((c + r) * 4 + 2, AWPic::Cursor, 0, tileX(c, r), tileY(c, r) + 16);
  addFronts(c, r);
}

// The 2 to 6 map blocks on screen.
static void prepareBlocks() {
  blockC = camX / 64;
  blockR = camY / 64;
  for (uint8_t br = 0; br < 2; br++) {
    FX::seekData(mapBlockTable + ((blockR + br) * blockCols + blockC) * 2);
    for (uint8_t bc = 0; bc < 3; bc++) blocks[br][bc] = fxRead16();
    FX::readEnd();
  }
}

// While the shop is open, its box hides the middle of the screen: columns 3 to 120, rows 15 to 60 are
// covered for sure (drawBox in shop). A picture 24 rows tall that lies inside is not drawn.
bool shopOpen;
static bool underShop(int16_t x, int16_t y, uint8_t w) {
  return shopOpen && x >= 3 && x <= WIDTH - 7 - w && y >= 15 && y <= HEIGHT - 3 - 24;
}

// The unit sheets unitsA, unitsB, unitsADone, unitsBDone must follow each other in fxdata.txt.
constexpr uint16_t UnitSheetSize = unitsB - unitsA;
static_assert(unitsADone - unitsB == UnitSheetSize && unitsBDone - unitsADone == UnitSheetSize, "unit sheets out of order in fxdata.txt");

static void drawFX(int16_t x, int16_t y, uint8_t w, uint8_t h, uint24_t image, uint8_t frame, uint8_t plane) {
  // graysprite pictures: 4 size bytes first, a frame per layer for each picture
  SpritesU::drawBasic(x, y, w, h, image + 4, frame * Planes + plane, SpritesU::MODE_PLUSMASKFX);
}

AW_FAST static void drawMap() {
  uint8_t plane = arduboy.currentPlane();
  for (uint8_t br = 0; br < 2; br++)
    for (uint8_t bc = 0; bc < 3; bc++) {
      int16_t x = (blockC + bc) * 64 - camX, y = (blockR + br) * 64 - camY;
      if (x < WIDTH && y < HEIGHT)
        SpritesU::drawBasic(x, y, 64, 64, mapBlocks, blocks[br][bc] * Planes + plane, SpritesU::MODE_OVERWRITEFX);
    }
  uint8_t phase = pictureCount >> 1;
  drawTiles(plane, 0, true);   // the fog
  if (rangeOn) drawTiles(plane, phase, false);
  if (arrowOn) drawArrow(plane);
  for (uint8_t i = 0; i < drawCount; i++) {
    AWDrawItem &d = drawItems[drawOrder[i]];
    int16_t x = d.x - camX, y = d.y - camY;
    if (x <= -48 || x >= WIDTH || y <= -48 || y >= HEIGHT) continue;
    if (underShop(x, y, d.pic >= AWPic::Mark ? 24 : 48)) continue;   // tops and the cursor are 48 wide
    uint8_t pic = uint8_t(d.pic), kind = d.frame & 15;
    switch (AWPic(pic & ~PicFortified)) {
      case AWPic::Top:
      case AWPic::DarkTop: {
        AWTop t;
        memcpy_P(&t, &mapTopTable[d.frame], sizeof(t));
        SpritesU::drawBasic(x + t.x, y + t.y, t.w, t.h, mapTops + t.start, plane, SpritesU::MODE_PLUSMASKFX);
        if (d.pic == AWPic::DarkTop) fogMask(x + t.x, y + t.y, t.w, t.h / 8, mapTops + t.start, plane);   // a top in fog
        break;
      }
      case AWPic::Cursor: drawFX(x, y, 48, 24, cursor, d.frame, plane); break;
      case AWPic::Mark:   drawFX(x, y, 8, 8, marks, d.frame, plane); break;
      case AWPic::Bar: break;
      default: {   // a unit: its sheet by side and done, the 4 sheets follow each other on the FX chip
        uint24_t sheet = unitsA;
        for (uint8_t n = (pic & ~PicFortified) - uint8_t(AWPic::UnitA); n; n--) sheet += UnitSheetSize;
        drawFX(x, y, 24, 24, sheet, kind, plane);
        break;
      }
    }
    if (pic & PicFortified) drawFX(x, y + 15, 24, 8, sandbags, 0, plane);
  }
  // A hurt unit shows its health at its bottom right, over everything, so a unit in front can not
  // hide it. Only where the number fits on screen: the letters do not clip.
  // A building under capture: a bar under its tile's unit, 1 pixel per point left.
  for (uint8_t i = 0; i < drawCount; i++) {
    AWDrawItem &d = drawItems[i];
    uint8_t health = d.frame >> 4;
    int16_t x = d.x - camX, y = d.y - camY;
    if (d.pic == AWPic::Bar) {
      SpritesU::fillRect(x + 13, y + 35, d.frame + 2, 4, 0);
      SpritesU::fillRect(x + 14, y + 36, d.frame, 2, 1);
      continue;
    }
    if (d.pic < AWPic::UnitA || health >= 10 || x < -20 || x > WIDTH - 24 || y < -19 || y > HEIGHT - 23 || underShop(x, y, 24)) continue;
    drawBadge(x + 18, y + 17, health);
  }
  drawTopBar();
}

// Once per picture: the D-pad pans the camera freely, the cursor is the tile under the screen center.
// 2 pixels per picture, 4 after holding the D-pad for half a second.
static void panCamera() {
  static uint8_t held;
  if (!arduboy.anyPressed(LEFT_BUTTON | RIGHT_BUTTON | UP_BUTTON | DOWN_BUTTON)) held = 0;
  else if (held < PicturesPerSecond / 2) held++;
  uint8_t speed = held < PicturesPerSecond / 2 ? 2 : 4;
  if (arduboy.pressed(LEFT_BUTTON)) camX -= speed;
  if (arduboy.pressed(RIGHT_BUTTON)) camX += speed;
  if (arduboy.pressed(UP_BUTTON)) camY -= speed;
  if (arduboy.pressed(DOWN_BUTTON)) camY += speed;
}

// Once per picture, after the camera moved: blocks, cursor and draw list.
static void updateMapView() {
  camX = constrain(camX, 0, (mapW + mapH) * 24 - WIDTH);
  camY = constrain(camY, 0, (mapW + mapH) * 12 + 24 - HEIGHT);
  uint8_t lastC = cursorC, lastR = cursorR;
  bool wasOn = cursorOn;
  findCursor();
  static int16_t lastCamX = -1, lastCamY;
  bool moved = camX != lastCamX || camY != lastCamY;
  lastCamX = camX;
  lastCamY = camY;
  if (moved) prepareBlocks();
  // The units' part of the draw list only when the camera left its area or units changed.
  if (mapChanged) prepareTopBar();   // whatever the bar shows changes together with the map
  if (mapChanged || abs(camX - winX0 - ListMargin) > ListMargin || abs(camY - winY0 - ListMargin) > ListMargin) {
    prepareList();
    prepareCursor();
  } else if (walking || cursorC != lastC || cursorR != lastR || cursorOn != wasOn) prepareCursor();
  mapChanged = false;
}

// Units or owners changed: the draw list is made again now.
static void refreshMap() {
  mapChanged = true;
  updateMapView();
}
