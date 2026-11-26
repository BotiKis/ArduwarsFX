// Made by tools/arrowart.py. Do not edit.
// The arrow's tiles in arrowTiles on the FX chip: 4 starts, 2 straights, 4 corners, 4 heads.
// Place in the 48x24 tile, size, start in arrowTiles. 2 frames (one per layer), with mask.
const AWTop arrowTable[] PROGMEM = {
  { 23, 8, 14, 16, 0 },
  { 12, 8, 14, 16, 112 },
  { 12, 0, 14, 16, 224 },
  { 23, 0, 14, 16, 336 },
  { 12, 0, 25, 24, 448 },
  { 12, 0, 25, 24, 748 },
  { 12, 8, 25, 16, 1048 },
  { 12, 0, 10, 24, 1248 },
  { 12, 0, 25, 16, 1368 },
  { 27, 0, 10, 24, 1568 },
  { 10, 0, 16, 16, 1688 },
  { 23, 0, 15, 16, 1816 },
  { 23, 8, 15, 16, 1936 },
  { 10, 8, 16, 16, 2056 },
};
