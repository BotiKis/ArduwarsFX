#ifndef MAPDATA_H
#define MAPDATA_H

// The layout of a map's header: byte offsets of each value.
// The maps live on the FX chip, in fxdata/fxdata.txt, with how to make one.
#define MAPDATAOFFSET_Size 0
#define MAPDATAOFFSET_Player1City 2
#define MAPDATAOFFSET_Player1Workshop 4
#define MAPDATAOFFSET_Player2City 6
#define MAPDATAOFFSET_Player2Workshop 8
#define MAPDATAOFFSET_Main 10

#endif
