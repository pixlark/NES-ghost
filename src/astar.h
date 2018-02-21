#ifndef NES_ASTAR_H
#define NES_ASTAR_H

#include <math.h>
#include <utility.h>
#include "heap.h"

List<Vector2i> a_star(
	const int * map, int width, int height,
	Vector2i start, Vector2i dest);

#endif
