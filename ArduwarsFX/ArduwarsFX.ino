// Arduwars FX: a turn-based strategy game for the Arduboy FX and FX Mini.
// Grayscale: ArduboyG on timer 1 with park row sync (the bottom pixel row stays black), 4 shades,
// 3 layers per picture, 52 pictures a second. Every layer redraws the whole screen.
// Art and maps live on the FX flash chip and are drawn with SpritesU.
#define ABG_IMPLEMENTATION
#define ABG_TIMER1
#define ABG_SYNC_PARK_ROW
#include "ArduboyG.h"
#include <ArduboyFX.h>
#define SPRITESU_IMPLEMENTATION
#define SPRITESU_FX
#define SPRITESU_PLUSMASK
#include "SpritesU.hpp"
#include "fxdata/fxdata.h"
#include "fxdata/mapTops.h"

ArduboyGBase_Config<ABG_Mode::L4_Triplane> arduboy;
ARDUBOY_NO_USB   // to upload, hold DOWN while switching on

#include "Match.h"
#include "MapView.h"

enum class AWScreen : uint8_t { Map };
AWScreen screen = AWScreen::Map;

// Dev readouts, top left. Hold A: work time per layer in microseconds (thinking and drawing), the worst
// of the last second. Hold A and B: layers that came late in the last second (more than 6.4 ms with sending).
uint16_t layerStart, worstTime, shownTime;
uint8_t pictures, lateLayers, shownLate;

static void drawNumber(uint16_t n) {
  // White 3x5 digits on black in the top left corner, straight into the screen buffer.
  static const uint8_t digits[] PROGMEM = {
    0x1f, 0x11, 0x1f,  0x12, 0x1f, 0x10,  0x1d, 0x15, 0x17,  0x15, 0x15, 0x1f,  0x07, 0x04, 0x1f,
    0x17, 0x15, 0x1d,  0x1f, 0x15, 0x1d,  0x01, 0x01, 0x1f,  0x1f, 0x15, 0x1f,  0x17, 0x15, 0x1f,
  };
  uint8_t *b = arduboy.getBuffer();
  for (uint8_t i = 0; i < 21; i++) b[i] = 0;
  for (uint8_t i = 5; i-- > 0; n /= 10) {
    const uint8_t *d = digits + (n % 10) * 3;
    for (uint8_t j = 0; j < 3; j++) b[1 + i * 4 + j] = pgm_read_byte(d + j) << 1;
  }
}

// ponytail: test units for step 1, 16 kinds per side on every other tile of an 8x4 area at each HQ.
// TestPacked 1: a 4x4 block instead, the busiest view for the draw time. Rules (step 2) replace this.
constexpr bool TestPacked = 0;
static void placeTestUnits() {
  for (uint8_t side = 0; side < 2; side++) {
    match.unitCount[side] = 16;
    for (uint8_t k = 0; k < 16; k++) {
      AWUnit &u = match.units[side][k];
      uint8_t c = TestPacked ? (k & 3) : (k & 3) * 2 + ((k >> 2) & 1), r = k >> 2;
      u.c = constrain(hqC[side] - 3 + c, 0, mapW - 1);
      u.r = constrain(hqR[side] - 1 + r, 0, mapH - 1);
      u.kind = k;
      u.health = 10;
    }
  }
}

void setup() {
  arduboy.boot();
  arduboy.flashlight();
  FX::begin(FX_DATA_PAGE);
  loadMap(0);
  placeTestUnits();
  FX::enableOLED();
  arduboy.startGray();
  FX::disableOLED();
}

void loop() {
  switch (screen) {
    case AWScreen::Map: drawMap(); break;
  }
  uint16_t took = micros() - layerStart;
  if (took > worstTime) worstTime = took;
  if (abg_detail::needs_display) lateLayers++;   // the timer already wants the next layer
  if (arduboy.pressed(A_BUTTON)) drawNumber(arduboy.pressed(B_BUTTON) ? shownLate : shownTime);

  // The screen and the FX chip share their wires: the screen listens only now.
  FX::enableOLED();
  arduboy.waitForNextPlane();
  FX::disableOLED();
  layerStart = micros();
  if (!arduboy.needsUpdate()) return;

  // Once per picture: read the buttons and think. This time counts toward the next layer.
  arduboy.pollButtons();
  if (++pictures == 52) {
    pictures = 0;
    shownTime = worstTime;
    worstTime = 0;
    shownLate = lateLayers;
    lateLayers = 0;
  }
  switch (screen) {
    case AWScreen::Map: updateMapView(); break;
  }
}
