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
int16_t originX, maxCamX, maxCamY;

int16_t camX, camY;       // map picture pixel at the top left of the screen
uint8_t cursorC, cursorR;   // the tile under the screen center
bool cursorOn;              // false when the screen center is off the map
uint8_t hqC[2], hqR[2];     // each player's HQ
bool mapChanged;          // set when units or owners change: the draw list is made again

// The 2 to 6 blocks on screen: block index in mapBlocks.
uint16_t blocks[2][3];
uint8_t blockC, blockR;   // first block column and row on screen

enum class AWPic : uint8_t { Top, Cursor, UnitA, UnitB, Mark };
struct AWDrawItem {
  uint8_t key;            // draw order: diagonal c+r times 4, then top 0, mark 1, cursor 2, unit 3
  AWPic pic;
  uint8_t frame;
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
int16_t listX = -1000, listY;      // camera when the units' part was made
int16_t winX0, winY0, winX1, winY1;   // map picture area the list covers

// Buildings in that area, for the tops in front of the cursor.
struct AWNearBuilding { uint8_t c, r, kind; int16_t x, y; };
constexpr uint8_t MaxNear = 16;
AWNearBuilding nearBuildings[MaxNear];
uint8_t nearCount;

// Big-endian reads from the FX chip, one byte at a time. ponytail: FX::readPendingUInt16/24 gave wrong
// values in this build (their inline assembly depends on where the compiler puts them).
static uint16_t fxRead16() { uint16_t v = FX::readPendingUInt8() << 8; return v | FX::readPendingUInt8(); }
static uint24_t fxRead24() { uint24_t v = uint24_t(fxRead16()) << 8; return v | FX::readPendingUInt8(); }

static int16_t tileX(uint8_t c, uint8_t r) { return (int16_t(c) - r) * 24 + originX; }
static int16_t tileY(uint8_t c, uint8_t r) { return (c + r) * 12; }

static void centerOn(uint8_t c, uint8_t r) {
  camX = tileX(c, r) + 24 - WIDTH / 2;
  camY = tileY(c, r) + 28 - HEIGHT / 2;
}

static void findCursor() {
  // Screen center to tile: undo the projection, relative to the top corner of tile (0, 0).
  int16_t dx = camX + WIDTH / 2 - (originX + 24);
  int16_t dy = camY + HEIGHT / 2 - 16;
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
  maxCamX = (mapW + mapH) * 24 - WIDTH;
  maxCamY = (mapW + mapH) * 12 + 24 - HEIGHT;

  // Start owners and HQs, and the cursor on player 1's HQ.
  match.map = index;
  FX::seekData(mapBuildings);
  for (uint8_t i = 0; i < buildingCount; i++) {
    uint8_t c = FX::readPendingUInt8();
    uint8_t r = FX::readPendingUInt8();
    uint8_t kindOwner = FX::readPendingUInt8();
    fxRead16();
    fxRead16();
    match.buildings[i].owner = kindOwner >> 4;
    if ((kindOwner & 15) == 0 && kindOwner >> 4) { hqC[(kindOwner >> 4) - 1] = c; hqR[(kindOwner >> 4) - 1] = r; }
  }
  FX::readEnd();
  centerOn(hqC[0], hqR[0]);
  findCursor();
}

AW_FAST static void addDrawItem(uint8_t key, AWPic pic, uint8_t frame, int16_t x, int16_t y) {
  if (x <= winX0 - 48 || x >= winX1 || y <= winY0 - 48 || y >= winY1) return;
  // ponytail: a full list drops the rest. 48 items fit a crowded screen and its margin.
  if (drawCount == drawLimit) return;
  drawItems[drawCount] = { key, pic, frame, x, y };
  uint8_t i = drawCount;
  for (; i > 0 && drawItems[drawOrder[i - 1]].key > key; i--) drawOrder[i] = drawOrder[i - 1];
  drawOrder[i] = drawCount++;
}

static void addTop(uint8_t key, uint8_t frame, int16_t x, int16_t y) {
  for (uint8_t i = 0; i < drawCount; i++) {
    AWDrawItem &d = drawItems[i];
    if (d.pic == AWPic::Top && d.key == key && d.x == x && d.frame == frame) return;   // already there
  }
  addDrawItem(key, AWPic::Top, frame, x, y);
}

// A unit or the cursor stands right behind tile (c, r): draw the tile's tree or mountain top again.
AW_FAST static void addFront(uint8_t c, uint8_t r) {
  uint16_t bit = r * 32 + c;
  if (c >= mapW || r >= mapH || !(tallBits[bit / 8] & (1 << (bit & 7)))) return;
  FX::seekData(mapGrid + r * mapW + c);
  addTop((c + r) * 4, (FX::readEnd() >> 4) - 1, tileX(c, r), tileY(c, r));
}

// Tiles with a unit in the list area, for the building tops.
constexpr uint8_t MaxStanding = 24;
uint8_t standC[MaxStanding], standR[MaxStanding];
uint8_t standCount;

static bool hasStanding(uint8_t c, uint8_t r) {
  for (uint8_t i = 0; i < standCount; i++)
    if (standC[i] == c && standR[i] == r) return true;
  return false;
}

static void setListArea(int16_t margin) {
  winX0 = camX - margin;
  winY0 = camY - margin;
  winX1 = camX + WIDTH + margin;
  winY1 = camY + HEIGHT + margin;
}

// The units' part of the draw list, when the camera left the area of the last one.
AW_FAST static void prepareList() {
  listX = camX;
  listY = camY;
  setListArea(ListMargin);
  drawCount = standCount = nearCount = 0;
  drawLimit = MaxListItems;
  // Tiles in the area, a little generous: diagonal c+r and screen column c-r.
  int8_t sLo = (winY0 - 48) / 12, sHi = winY1 / 12;
  int8_t dLo = (winX0 - originX - 48) / 24 - 1, dHi = (winX1 - originX) / 24 + 1;
  for (uint8_t side = 0; side < 2; side++)
    for (uint8_t i = 0; i < match.unitCount[side]; i++) {
      AWUnit &u = match.units[side][i];
      int8_t c = u.c, r = u.r;
      if (c + r < sLo || c + r > sHi || c - r < dLo || c - r > dHi) continue;
      // unit bottom 6 pixels below the diamond center
      uint8_t before = drawCount;
      addDrawItem((c + r) * 4 + 3, side ? AWPic::UnitB : AWPic::UnitA, u.kind, tileX(c, r) + 12, tileY(c, r) + 10);
      if (drawCount == before) continue;
      if (standCount < MaxStanding) { standC[standCount] = c; standR[standCount++] = r; }
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
    // ponytail: past 16 buildings in the area, the cursor gets no building tops. A screen shows about 6.
    if (nearCount < MaxNear) nearBuildings[nearCount++] = { c, r, kind, x, y };
    uint8_t owner = match.buildings[i].owner;
    if (owner) addDrawItem((c + r) * 4 + 1, AWPic::Mark, owner - 1, x + 20, y + 34);   // at the front corner
    if (hasStanding(c - 1, r) || hasStanding(c, r - 1) || hasStanding(c - 1, r - 1))
      addDrawItem((c + r) * 4, AWPic::Top, kind, x, y);
  }
  FX::readEnd();
  listCount = drawCount;
}

// The cursor's part of the draw list, each picture the cursor changed tile and after prepareList.
AW_FAST static void prepareCursor() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < drawCount; i++)
    if (drawOrder[i] < listCount) drawOrder[n++] = drawOrder[i];
  drawCount = listCount;
  drawLimit = MaxDrawItems;
  if (!cursorOn) return;
  uint8_t c = cursorC, r = cursorR;
  addDrawItem((c + r) * 4 + 2, AWPic::Cursor, 0, tileX(c, r), tileY(c, r) + 16);
  addFront(c + 1, r);
  addFront(c, r + 1);
  addFront(c + 1, r + 1);
  for (uint8_t i = 0; i < nearCount; i++) {
    AWNearBuilding &b = nearBuildings[i];
    if ((b.c == c + 1 && b.r == r) || (b.c == c && b.r == r + 1) || (b.c == c + 1 && b.r == r + 1))
      addTop((b.c + b.r) * 4, b.kind, b.x, b.y);
  }
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

static void drawFX(int16_t x, int16_t y, uint8_t w, uint8_t h, uint24_t image, uint8_t frame, uint8_t plane) {
  // graysprite pictures: 4 size bytes first, 3 frames per picture, one per layer
  SpritesU::drawBasic(x, y, w, h, image + 4, frame * 3 + plane, SpritesU::MODE_PLUSMASKFX);
}

static void drawMap() {
  uint8_t plane = arduboy.currentPlane();
  for (uint8_t br = 0; br < 2; br++)
    for (uint8_t bc = 0; bc < 3; bc++) {
      int16_t x = (blockC + bc) * 64 - camX, y = (blockR + br) * 64 - camY;
      if (x < WIDTH && y < HEIGHT)
        SpritesU::drawBasic(x, y, 64, 64, mapBlocks, blocks[br][bc] * 3 + plane, SpritesU::MODE_OVERWRITEFX);
    }
  for (uint8_t i = 0; i < drawCount; i++) {
    AWDrawItem &d = drawItems[drawOrder[i]];
    int16_t x = d.x - camX, y = d.y - camY;
    if (x <= -48 || x >= WIDTH || y <= -48 || y >= HEIGHT) continue;
    switch (d.pic) {
      case AWPic::Top: {
        AWTop t;
        memcpy_P(&t, &mapTopTable[d.frame], sizeof(t));
        SpritesU::drawBasic(x + t.x, y + t.y, t.w, t.h, mapTops + t.start, plane, SpritesU::MODE_PLUSMASKFX);
        break;
      }
      case AWPic::Cursor: drawFX(x, y, 48, 24, cursor, d.frame, plane); break;
      case AWPic::UnitA:  drawFX(x, y, 24, 24, unitsA, d.frame, plane); break;
      case AWPic::UnitB:  drawFX(x, y, 24, 24, unitsB, d.frame, plane); break;
      case AWPic::Mark:   drawFX(x, y, 8, 8, marks, d.frame, plane); break;
    }
  }
}

// Once per picture: the D-pad pans the camera freely, the cursor is the tile under the screen center.
static void updateMapView() {
  uint8_t speed = arduboy.pressed(B_BUTTON) ? 4 : 2;   // pixels per picture
  if (arduboy.pressed(LEFT_BUTTON)) camX -= speed;
  if (arduboy.pressed(RIGHT_BUTTON)) camX += speed;
  if (arduboy.pressed(UP_BUTTON)) camY -= speed;
  if (arduboy.pressed(DOWN_BUTTON)) camY += speed;
  camX = constrain(camX, 0, maxCamX);
  camY = constrain(camY, 0, maxCamY);
  uint8_t lastC = cursorC, lastR = cursorR;
  bool wasOn = cursorOn;
  findCursor();
  static int16_t lastCamX = -1, lastCamY;
  if (camX != lastCamX || camY != lastCamY) prepareBlocks();
  lastCamX = camX;
  lastCamY = camY;
  // The units' part of the draw list only when the camera left its area or units changed.
  if (mapChanged || abs(camX - listX) > ListMargin || abs(camY - listY) > ListMargin) {
    prepareList();
    prepareCursor();
  } else if (cursorC != lastC || cursorR != lastR || cursorOn != wasOn) prepareCursor();
  mapChanged = false;
}
