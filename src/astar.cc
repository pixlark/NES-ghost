#include "astar.h"

#define SQR(a) (a) * (a)

Vector2i dirs[4] = {
	{-1, +0}, // left
	{+1, +0}, // right
	{+0, -1}, // up
	{+0, +1}, // down
};

struct Vert {
	Vector2i pos;
	float dist;
	float heur;
	int viewed_index;
};

bool vert_compare(Vert a, Vert b)
{
	return (a.dist + a.heur) < (b.dist + b.heur);
}

bool vert_equality(Vert a, Vert b)
{
	return a.pos.x == b.pos.x && a.pos.y == b.pos.y;
}

#define DEBUG_MODE 0
List<Vector2i> a_star(
	const int * map, int width, int height,
	Vector2i start, Vector2i dest)
{
	float * heu_map = (float*) malloc(sizeof(float) * width * height);
	if (DEBUG_MODE) printf("Heuristic map:\n");
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			heu_map[x+y*width] = sqrt(SQR(y - dest.y) + SQR(x - dest.x));
			if (DEBUG_MODE) printf("%.2f ", heu_map[x+y*width]);
		}
		if (DEBUG_MODE) printf("\n");
	}
	
	List<Vector2i> directions;
	directions.alloc();

	List<Vert> heap;
	heap.alloc();
	List<Vert> viewed;
	viewed.alloc();
	Vert start_v;
	start_v.pos  = start;
	start_v.dist = 0;
	start_v.heur = heu_map[start.x+start.y*width];
	start_v.viewed_index = -1;
	heap_insert(&heap, start_v, vert_compare);
	while (heap.len > 0) {
		auto check = [map, width, height](Vector2i t) {
			if (t.x < 0 || t.x >= width  ||
				t.y < 0 || t.y >= height ||
				map[t.x+t.y*width]) {
				return false;
			}
			return true;
		};
		Vert v = heap_pop(&heap, vert_compare);
		if (v.pos.x == dest.x && v.pos.y == dest.y) {
			if (viewed.len == 0) break;
			Vert e = v;
			Vert b = viewed[v.viewed_index];
			do {
				directions.push(Vector2i(e.pos.x - b.pos.x, e.pos.y - b.pos.y));
				Vert temp = b;
				b = viewed[b.viewed_index];
				e = temp;
			} while (e.viewed_index != -1);
			break;
		}
		for (int i = 0; i < 4; i++) {
			Vert nv;
			nv.pos  = v.pos + dirs[i];
			nv.dist = sqrt(SQR(nv.pos.y - dest.y) + SQR(nv.pos.x - dest.x));
			nv.heur = heu_map[nv.pos.x+nv.pos.y*width];
			nv.viewed_index = viewed.len;
			if (check(nv.pos) &&
				heap  .find(nv, vert_equality) == -1 &&
				viewed.find(nv, vert_equality) == -1) {
				if (DEBUG_MODE) printf("%d %d\n", nv.pos.x, nv.pos.y);
				heap_insert(&heap, nv, vert_compare);
			}
		}
		viewed.push(v);
	}
	heap.dealloc();
	viewed.dealloc();
	free(heu_map);
	directions.reverse();
	return directions;
}
