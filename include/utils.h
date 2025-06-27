#pragma once

#include <math.h>
#include <stdint.h>

// For calculating sector at a position in the spiral track/groove
inline int trackToSector(const int track) { return pow(track, 2) * 0.00031499 + track * 9.357516535; }

inline int sectorsPerTrack(const int track) { return round(track * 0.000616397 + 9); }