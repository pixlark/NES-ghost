#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <math.h>
#include <render.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <utility.h>

#include "astar.h"
#include "heap.h"

#define REFACTOR 0

#if REFACTOR
struct Texture {
	Vector2i pos;
	Vector2i dim;
	Vector2f scale;
};

struct Entity {
	Vector2i pos;
	Texture  tex;
};

struct Window {
	SDL_Window * sdl;
	uint32_t last_time;
	float delta_time = 0.0167;
};

void draw_entity(Entity * e)
{
	Render::render(e->pos, e->tex.pos, e->tex.dim, e->tex.scale);
}

Window get_window()
{
	Vector2i base_res(256, 240);
	float res_scale = 4.0;
	Window window;
	window.sdl = SDL_CreateWindow("NES game",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		base_res.x * res_scale, base_res.y * res_scale,
		SDL_WINDOW_SHOWN|SDL_WINDOW_OPENGL);
	{
		List<char> exe_path = get_exe_dir();
		exe_path.cat("..\\atlas.png", 13, 1);
		printf("Loading atlas from %s\n", exe_path.arr);
	
		Render::init(window.sdl, exe_path.arr, base_res, res_scale);

		exe_path.dealloc();
	}
	window.last_time = SDL_GetPerformanceCounter();
}

void tick_delta_time(Window * w)
{
	// Manage framerate
	{
		uint32_t new_time = SDL_GetPerformanceCounter();
		w->delta_time = (float) (new_time - w->last_time) / SDL_GetPerformanceFrequency();
		w->last_time = new_time;
	}
	if (w->delta_time < 1.0 / 60.0) {
		SDL_Delay(((1.0 / 60.0) - w->delta_time) * 1000);
	}
}

int main()
{
	srand(time(NULL));
	
	SDL_Init(SDL_INIT_VIDEO);
	Window window = get_window();
	
	SDL_Event event;
	bool running = true;
	while (running) {
		while (SDL_PollEvent(&event) != 0) {
			switch (event.type) {
			case SDL_QUIT:
				running = false;
			}
		}
		Render::clear(RGBA(36, 56, 225, 255));
		Render::render(Vector2i(0, 0), Vector2i(0, 0), Vector2i(16, 16), Vector2f(1, 1));
		Render::swap(window.sdl);
		tick_delta_time(&window);
	}
	return 0;
}

#else

#define PLAY_W 16
#define PLAY_H 13

float delta_time = 0.0167;

inline int to_index(Vector2i pos, int w = PLAY_W)
{
	return pos.x + pos.y * w;
}

inline int to_index(int x, int y, int w = PLAY_W)
{
	return x + y * PLAY_W;
}

Vector2i directions[] = {
	{+0, -1}, // UP
	{-1, +0}, // LEFT
	{+0, +1}, // DOWN
	{+1, +0}, // RIGHT
};

// TODO(pixlark): Implement a binary search here or something
// TODO(pixlark): Also make this a method on the Vector2
bool contains(List<Vector2i> * l, Vector2i v)
{
	for (int i = 0; i < l->len; i++) {
		if (l->arr[i].x == v.x && l->arr[i].y == v.y) return true;
	}
	return false;
}

enum Power {
	NONE,
	BLUE,
	GREEN,
};

struct Sprite {
	Vector2i pos;
	Vector2i tex_pos;
	Vector2i tex_size;
	Vector2f scale;
	void init(Vector2i pos, Vector2i tex_pos, Vector2i tex_size)
	{
		this->pos = pos;
		this->tex_pos = tex_pos;
		this->tex_size = tex_size;
		this->scale = Vector2f(1, 1);
	}
	void render(Vector2i offset = {0, 0})
	{
		Render::render(pos, tex_pos + offset, tex_size, scale);
	}
};

struct Level {
	int level[PLAY_W * PLAY_H];
	Vector2i crystal_pos;
	void add_walls(Vector2i pos, List<Vector2i> * walls)
	{
		for (int i = 0; i < 4; i++) {
			Vector2i n = pos + directions[i];
			if (level[to_index(n)]) {
				if (!contains(walls, n) &&
					n.x >= 1 && n.x < PLAY_W-1 && n.y >= 1 && n.y < PLAY_H-1) {
					walls->push(n);
				}
			}
		}
	}

	void generate_level(bool top_left)
	{
		/* This doesn't quite work. I think we need to union-find on
		   the crystal position and iterate tunneling until they
		   connect.
             -Paul T. Sun Feb 11 14:44:15 2018 */
		auto tunnel_from_point = [this](int level[PLAY_W * PLAY_H], Vector2i p) {
			List<Vector2i> walls;
			level[to_index(p)] = 0;
			walls.alloc();
			add_walls(p, &walls);
			while (1) {
				if (walls.len == 0) break;
				int wi = rand() % walls.len;
				Vector2i w = walls[wi];
				int bordering = 0;
				for (int i = 0; i < 4; i++) {
					if (!level[to_index(w + directions[i])]) {
						bordering++;
					}
				}
				if (bordering == 1) {
					level[to_index(w)] = 0;
					add_walls(w, &walls);
				}
				walls.remove(wi);
			}
			walls.dealloc();
		};
		Vector2i start;
		if (top_left) {
			start.x = 1;
			start.y = 1;
			crystal_pos.x = PLAY_W - 2;
			crystal_pos.y = PLAY_H - 2;
		} else {
			start.x = PLAY_W - 2;
			start.y = PLAY_H - 2;
			crystal_pos.x = 1;
			crystal_pos.y = 1;
		}
		for (int i = 0; i < PLAY_W*PLAY_H; i++) level[i] = 1;
		tunnel_from_point(level, start);
		{
			Vector2i dir = top_left ? Vector2i(-1, -1) : Vector2i(1, 1);
			while (level[to_index(crystal_pos)]) {
				// TODO(pixlark): This has about a 1/2048 chance (I
				// think) of failing if the maze generation forms a
				// perfect diagonal line.
				crystal_pos += dir;
			}
		}
		printf("%d %d\n", crystal_pos);
	}
};

struct Actor {
	const float time_max = 0.5;
	const float speed = 25;
	Vector2f real_pos;
	float time = 0;
	Vector2i grid_pos;
	Vector2i target_pos;
	int power;
	int selected_power;
	Sprite sprite;
	bool moving = false;
	void init(Vector2i pos, Vector2i tex_pos, Vector2i tex_size)
	{
		power = NONE;
		selected_power = power;
		grid_pos = pos;
		target_pos = grid_pos;
		sprite.init(Vector2i(pos.x * 16, pos.y * 16), tex_pos, tex_size);
		real_pos = Vector2f(pos.x * 16, pos.y * 16);
	}
	bool update(Level * level, Vector2i direction)
	{
		bool finished = false;
		if (!moving) {
			if (direction.x != 0 || direction.y != 0) {
				moving = true;
				time = time_max;
			}
		}
		if (level->level[to_index(target_pos)]) {
			target_pos = grid_pos + direction;
		} else {
			time += delta_time;
			float t = time / time_max;
			real_pos.x = (grid_pos.x * 16) + t * ((target_pos.x - grid_pos.x) * 16);
			real_pos.y = (grid_pos.y * 16) + t * ((target_pos.y - grid_pos.y) * 16);
			if (t >= 1) {
				time = 0;
				grid_pos = target_pos;
				real_pos.x = grid_pos.x * 16;
				real_pos.y = grid_pos.y * 16;
				target_pos = grid_pos + direction;
				finished = true;
			}
			sprite.pos = (Vector2i) real_pos;
		}
		return finished;
	}
};

void level_to_walls(Level * level, List<Sprite> * walls) {
	walls->dealloc();
	walls->alloc();
	for (int y = 0; y < PLAY_H; y++) {
		for (int x = 0; x < PLAY_W; x++) {
			if (level->level[to_index(x, y)]) {
				Sprite sprite;
				sprite.init(Vector2i(x * 16, y * 16), Vector2i(32, 0), Vector2i(16, 16));
				walls->push(sprite);
			}
		}
	}
}

int main()
{
	srand(time(NULL));
	Vector2i base_res(256, 240);
	float res_scale = 4.0;
	
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window * window = SDL_CreateWindow("NES game",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		base_res.x * res_scale, base_res.y * res_scale,
		SDL_WINDOW_SHOWN|SDL_WINDOW_OPENGL);

	{
		List<char> exe_path = get_exe_dir();
		exe_path.cat("..\\atlas.png", 13, 1);
		printf("Loading atlas from %s\n", exe_path.arr);
	
		Render::init(window, exe_path.arr, base_res, res_scale);

		exe_path.dealloc();
	}

	Vector2i start_pos = {1, 1};
	
	Level level;
	bool top_left = true;
	level.generate_level(top_left);

	List<Sprite> walls;
	level_to_walls(&level, &walls);

	Actor player;
	player.init(start_pos, Vector2i(32, 16), Vector2i(16, 16));
	Vector2i queued_direction = Vector2i(0, 0);

	Actor ghost;
	{
		Vector2i ghost_pos;
		do {
			ghost_pos.x = rand() % PLAY_W;
			ghost_pos.y = rand() % PLAY_H;
		} while (level.level[to_index(ghost_pos)]);
		ghost.init(ghost_pos, Vector2i(48, 48), Vector2i(16, 16));
	}
	Vector2i ghost_direction = Vector2i(0, 0);
	
	Sprite barrier;
	barrier.init(Vector2i(0, base_res.y - 48), Vector2i(0, 0), Vector2i(16, 16));
	barrier.scale = Vector2f(16, 1);

	uint32_t last_time = SDL_GetPerformanceCounter();
	
	SDL_Event event;
	bool running = true;
	while (running) {
		while (SDL_PollEvent(&event) != 0) {
			switch (event.type) {
			case SDL_QUIT:
				running = false;
				break;
			case SDL_KEYDOWN: {
				switch (event.key.keysym.scancode) {
				case SDL_SCANCODE_W: {
					queued_direction = Vector2i(+0, -1);
				} break;
				case SDL_SCANCODE_A: {
					queued_direction = Vector2i(-1, +0);
				} break;
				case SDL_SCANCODE_S: {
					queued_direction = Vector2i(+0, +1);
				} break;
				case SDL_SCANCODE_D: {
					queued_direction = Vector2i(+1, +0);
				} break;
				case SDL_SCANCODE_LEFT: {
					if (player.selected_power > 1) {
						player.selected_power--;
					}
				} break;
				case SDL_SCANCODE_RIGHT: {
					if (player.selected_power < player.power) {
						player.selected_power++;
					}
				};
				}
			} break;
			}
		}
		// Update
		player.update(&level, queued_direction);
		if (player.grid_pos.x == level.crystal_pos.x && player.grid_pos.y == level.crystal_pos.y) {
			player.power++;
			player.selected_power = player.power;
			queued_direction = Vector2i(0, 0);
			player.moving = false;
			top_left = !top_left;
			level.generate_level(top_left);
			level_to_walls(&level, &walls);
			{
				Vector2i ghost_pos;
				do {
					ghost_pos.x = rand() % PLAY_W;
					ghost_pos.y = rand() % PLAY_H;
				} while (level.level[to_index(ghost_pos)]);
				ghost.init(ghost_pos, Vector2i(48, 48), Vector2i(16, 16));
			}
		}
		{
			if (ghost.update(&level, directions[0])) {
				List<Vector2i> directions = a_star(level.level, PLAY_W, PLAY_H, ghost.target_pos, player.grid_pos);
				if (directions.len > 0) {
					ghost_direction = directions[0];
				}
				directions.dealloc();
			}
		}
		// Render
		Render::clear(RGBA(36, 56, 225, 255));
		for (int i = 0; i < walls.len; i++) {
			walls[i].render();
		}
		Render::render(
			Vector2i(level.crystal_pos.x * 16, level.crystal_pos.y * 16),
			Vector2i(player.power * 16 + 48, 0), Vector2i(16, 16), Vector2f(1, 1));
		ghost.sprite.render();
		player.sprite.render(Vector2i(player.selected_power * 16, 0));
		for (int i = 0; i < 8; i++) {
			if (i == player.selected_power - 1) {
				Render::render(Vector2i(i * 32, base_res.y - 32), Vector2i(0, 48), Vector2i(32, 32), Vector2i(1, 1));
			} else {
				Render::render(Vector2i(i * 32, base_res.y - 32), Vector2i(0, 16), Vector2i(32, 32), Vector2i(1, 1));
			}
		}
		for (int i = 0; i < player.power; i++) {
			Render::render(Vector2i(i * 32 + 8, base_res.y - 24), Vector2i(i * 16 + 48, 0), Vector2i(16, 16), Vector2i(1, 1));
		}
		barrier.render();
		Render::swap(window);
		// Manage framerate
		{
			uint32_t new_time = SDL_GetPerformanceCounter();
			delta_time = (float) (new_time - last_time) / SDL_GetPerformanceFrequency();
			last_time = new_time;
		}
		if (delta_time < 1.0 / 60.0) {
			// Lock to 60
			SDL_Delay(((1.0 / 60.0) - delta_time) * 1000);
		}
	}
	
	return 0;
}
#endif
