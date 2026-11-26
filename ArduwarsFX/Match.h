#pragma once
// The state of a running match, in one compact block. A save game (todo 5) stores this block as it is.
// Everything that never changes during a match (terrain, building places and kinds) stays on the FX chip.

constexpr uint8_t MaxUnits = 24;      // per side
constexpr uint8_t MaxBuildings = 48;  // per map, all owners
constexpr uint8_t MaxMapSize = 32;    // width and height

enum class AWOwner : uint8_t { Neutral, Player1, Player2 };

// 3 bytes per unit. A bitfield never crosses a byte here, see the comment in the old DataClasses.h.
struct AWUnit {
  uint8_t c : 5;          // grid column
  uint8_t moved : 1;
  uint8_t fortified : 1;
  uint8_t : 1;
  uint8_t r : 5;          // grid row
  uint8_t : 3;
  uint8_t kind : 4;       // unit kind, game order (Soldier first)
  uint8_t health : 4;
};

// 1 byte per building. Building i is building i of the map on the FX chip.
struct AWBuilding {
  uint8_t owner : 2;      // AWOwner
  uint8_t capture : 6;    // capture points left
};

struct AWMatch {
  uint8_t map;            // index in the map list
  uint8_t day;
  uint8_t side;           // whose turn: 0 player 1, 1 player 2
  uint16_t money[2];
  uint8_t unitCount[2];
  AWUnit units[2][MaxUnits];
  AWBuilding buildings[MaxBuildings];
};

AWMatch match;
