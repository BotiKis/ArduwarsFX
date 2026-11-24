#ifndef UNITSPRITES_H
#define UNITSPRITES_H

#include <avr/pgmspace.h>

// Unit sprites, 16x16, one picture per unit, frame = unitType.
// The pictures live on the FX chip: unitsA_upper, unitsA_lower, unitsB_upper,
// unitsB_lower in fxdata/units.txt. This file keeps the ground shadows.
// Each unit is cut in two parts so it can bob (see AWGame::drawUnit):
// the upper part holds rows 0-8 (16x16 frame, rows 9-15 empty),
// the lower part holds rows 9-15 (16x8 frame, last row empty).
// unitsA is player 1, unitsB is player 2 (faces left).
// Made from Assets/Gamesprites/units*_16x16.png, one 16x16 picture per unit.
// A bottom row of only black pixels in one line is a ground shadow: it is left
// out of the pictures and listed in units*_shadows, drawn by AWGame::drawUnit.

// Ground shadow per unit: start x << 4 | length, 0 = none. See AWGame::drawUnit.
const unsigned char unitsA_shadows[] PROGMEM =
{
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b, 0x19, 0x00, 0x00, 0x00, 0x00
};

// Ground shadow per unit: start x << 4 | length, 0 = none. See AWGame::drawUnit.
const unsigned char unitsB_shadows[] PROGMEM =
{
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4b, 0x78, 0x00, 0x00, 0x00, 0x00
};

#endif
