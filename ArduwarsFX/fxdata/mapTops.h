// Made by tools/mapconv.py. Do not edit.
// Tops in mapTops on the FX chip: building kinds first, then trees and mountains.
// Place in the 48x48 tile picture, size, start in mapTops. 2 frames (one per layer), with mask.
struct AWTop { uint8_t x, y, w, h; uint16_t start; };
const AWTop mapTopTable[] PROGMEM = {
  { 10, 0, 29, 24, 0 },
  { 12, 5, 25, 16, 348 },
  { 8, 0, 33, 24, 548 },
  { 14, 12, 4, 8, 944 },
  { 10, 6, 29, 16, 960 },
  { 8, 7, 31, 16, 1192 },
  { 10, 6, 22, 16, 1440 },
};
