// Made by tools/titlelayers.py. Do not edit.
// The start screen's layers in titleLayers on the FX chip, back to front: place in the 160x64 scene,
// size, start in titleLayers, kind (0 no mask, 1 mask, 2 pixel list).
struct AWTitleLayer { uint8_t x, y, w, h; uint16_t start; uint8_t kind; };
constexpr uint8_t TitleLayers = 8;
const AWTitleLayer titleTable[] PROGMEM = {
  { 0, 0, 160, 64, 0, 0 },   // title-01-sky-160x64.png
  { 0, 32, 160, 32, 2560, 1 },   // title-02-ground-160x64.png
  { 47, 0, 71, 56, 5120, 1 },   // title-03-dust-160x64.png
  { 0, 0, 131, 48, 7108, 2 },   // title-04-backdust-160x64.png
  { 17, 24, 130, 40, 7288, 1 },   // title-05-troops-160x64.png
  { 58, 8, 47, 56, 9888, 1 },   // title-06-tank-160x64.png
  { 0, 40, 160, 24, 11204, 1 },   // title-07-frontdust-160x64.png
  { 50, 0, 75, 24, 13124, 1 },   // title-08-logo-160x64.png
};
