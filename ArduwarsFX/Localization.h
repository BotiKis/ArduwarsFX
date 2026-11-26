#pragma once
// All text on screen. Letters: Tinyfont's 4x4 capitals, digits and a few signs.
using char_P = const char;

char_P LOCA_player1[] PROGMEM   = "PLAYER 1";
char_P LOCA_player2[] PROGMEM   = "PLAYER 2";
char_P LOCA_endTurn[] PROGMEM   = "END TURN";
char_P LOCA_unitsLeft[] PROGMEM = " LEFT";   // after the number of units that can still act
char_P LOCA_wait[] PROGMEM      = "WAIT";
char_P LOCA_cancel[] PROGMEM    = "CANCEL";
char_P LOCA_cursor[] PROGMEM    = ">";
char_P LOCA_attack[] PROGMEM    = "ATTACK";
char_P LOCA_fortify[] PROGMEM   = "FORTIFY";
char_P LOCA_capture[] PROGMEM   = "CAPTURE";
char_P LOCA_captured[] PROGMEM  = "CAPTURED";
char_P LOCA_ambush[] PROGMEM    = "AMBUSH";
char_P LOCA_p1Wins[] PROGMEM    = "PLAYER 1 WINS";
char_P LOCA_p2Wins[] PROGMEM    = "PLAYER 2 WINS";
char_P LOCA_draw[] PROGMEM      = "DRAW";
// Top bar and shop
char_P LOCA_p1[] PROGMEM        = "P1";
char_P LOCA_p2[] PROGMEM        = "P2";
char_P LOCA_day[] PROGMEM       = "DAY:";
char_P LOCA_money[] PROGMEM     = "M:";
char_P LOCA_hundreds[] PROGMEM  = "00";
char_P LOCA_noBuy[] PROGMEM     = "X";     // after a unit the player can not buy
char_P LOCA_attackShort[] PROGMEM = "ATK";
char_P LOCA_speed[] PROGMEM     = "SPEED";
// Unit names, game order
char_P LOCA_unit0[] PROGMEM  = "SOLDIER";
char_P LOCA_unit1[] PROGMEM  = "MECH";
char_P LOCA_unit2[] PROGMEM  = "SPECOPS";
char_P LOCA_unit3[] PROGMEM  = "SCOUT";
char_P LOCA_unit4[] PROGMEM  = "ASSIST";
char_P LOCA_unit5[] PROGMEM  = "TANK";
char_P LOCA_unit6[] PROGMEM  = "BIGTANK";
char_P LOCA_unit7[] PROGMEM  = "ARTILLERY";
char_P LOCA_unit8[] PROGMEM  = "ROCKET";
char_P LOCA_unit9[] PROGMEM  = "MISSILES";
char_P LOCA_unit10[] PROGMEM = "HELI";
char_P LOCA_unit11[] PROGMEM = "FIGHTER";
char_P LOCA_unit12[] PROGMEM = "BOMBER";
char_P LOCA_unit13[] PROGMEM = "CRUISER";
char_P LOCA_unit14[] PROGMEM = "BATTLESHIP";
char_P * const LOCA_unitNames[15] PROGMEM = {
  LOCA_unit0, LOCA_unit1, LOCA_unit2, LOCA_unit3, LOCA_unit4, LOCA_unit5, LOCA_unit6, LOCA_unit7,
  LOCA_unit8, LOCA_unit9, LOCA_unit10, LOCA_unit11, LOCA_unit12, LOCA_unit13, LOCA_unit14,
};
