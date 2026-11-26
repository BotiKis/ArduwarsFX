// Arduwars FX: a turn-based strategy game for the Arduboy FX and FX Mini.
// Grayscale: ArduboyG on timer 1 with park row sync (the bottom pixel row stays black), 4 shades,
// 3 layers per picture, 100 layers (33 pictures) a second. Every layer redraws the whole screen.
// Art and maps live on the FX flash chip and are drawn with SpritesU.
#define ABG_IMPLEMENTATION
#define ABG_TIMER1
#define ABG_SYNC_PARK_ROW
// 100 layers a second: 8.8 ms per layer to think and draw. The fog edge costs about 0.4 ms per piece,
// at 156 (ArduboyG's default, 5.2 ms) most views with fog came late. More gray flicker on the device.
#define ABG_REFRESH_HZ 100
constexpr uint8_t PicturesPerSecond = ABG_REFRESH_HZ / 3;
#include "ArduboyG.h"
#include <ArduboyFX.h>
#define SPRITESU_IMPLEMENTATION
#define SPRITESU_FX
#define SPRITESU_PLUSMASK
#define SPRITESU_RECT
#include "SpritesU.hpp"
#include "fxdata/fxdata.h"
#include "fxdata/mapTops.h"

ArduboyGBase_Config<ABG_Mode::L4_Triplane> arduboy;
ARDUBOY_NO_USB   // to upload, hold DOWN while switching on

#include "Match.h"
#include "MapView.h"
#include "Rules.h"

// Dev readouts, top left, when switched on here: the worst work time per layer in microseconds
// (thinking and drawing) in the last second, then the layers that came late in the last second
// (more than 10 ms with sending). Must stay 0.
constexpr bool DevReadouts = false;
uint16_t layerStart, worstTime, shownTime;
uint8_t pictures, lateLayers, shownLate;

static void drawNumber(uint8_t x, uint16_t n) {
  // White 3x5 digits on black at the top, straight into the screen buffer.
  static const uint8_t digits[] PROGMEM = {
    0x1f, 0x11, 0x1f,  0x12, 0x1f, 0x10,  0x1d, 0x15, 0x17,  0x15, 0x15, 0x1f,  0x07, 0x04, 0x1f,
    0x17, 0x15, 0x1d,  0x1f, 0x15, 0x1d,  0x01, 0x01, 0x1f,  0x1f, 0x15, 0x1f,  0x17, 0x15, 0x1f,
  };
  uint8_t *b = arduboy.getBuffer() + x;
  for (uint8_t i = 0; i < 21; i++) b[i] = 0;
  for (uint8_t i = 5; i-- > 0; n /= 10) {
    const uint8_t *d = digits + (n % 10) * 3;
    for (uint8_t j = 0; j < 3; j++) b[1 + i * 4 + j] = pgm_read_byte(d + j) << 1;
  }
}

static void prepareTopBar();   // in Turn.h

// Sends the layer just drawn. Returns true once per picture, after reading the buttons: then think.
// The thinking time counts toward the next layer.
static bool sendLayer() {
  uint16_t took = micros() - layerStart;
  if (took > worstTime) worstTime = took;
  if (abg_detail::needs_display) lateLayers++;   // the timer already wants the next layer
  if (DevReadouts) { drawNumber(0, shownTime); drawNumber(22, shownLate); }

  // The screen and the FX chip share their wires: the screen listens only now.
  FX::enableOLED();
  arduboy.waitForNextPlane();
  FX::disableOLED();
  layerStart = micros();
  if (!arduboy.needsUpdate()) return false;
  arduboy.pollButtons();
  pictureCount++;
  prepareTopBar();
  if (++pictures == PicturesPerSecond) {
    pictures = 0;
    shownTime = worstTime;
    worstTime = 0;
    shownLate = lateLayers;
    lateLayers = 0;
  }
  return true;
}

#include "Turn.h"

void setup() {
  arduboy.boot();
  arduboy.flashlight();
  FX::begin(FX_DATA_PAGE);
  FX::enableOLED();
  arduboy.startGray();
  FX::disableOLED();
  startMatch();
}

void loop() {
  drawMap();
  if (sendLayer()) updateTurn();
}
