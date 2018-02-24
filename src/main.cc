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

Vector2i directions[] = {
	{+0, -1}, // UP
	{-1, +0}, // LEFT
	{+0, +1}, // DOWN
	{+1, +0}, // RIGHT
};

struct Texture {
	Vector2i pos;
	Vector2i dim;
	Vector2f scale;
};

enum EntityType {
	ENTITY,
	PLAYER,
	GHOST,
};

struct Entity {
	Vector2i pos;
	Vector2i grid_pos;
	Vector2i target_pos;
	float move_t;
	bool moving;
	bool visible = true;
	Texture  tex;
	EntityType type;
};

struct Player : Entity {
	int power_max;
	int power_level;
	Vector2i direction;
	Vector2i queued_direction;
};

enum GhostState {
	GHOST_ALIVE,
	GHOST_DEAD,
};

#define GHOST_DEATH_TIMER_RESET 1.0
#define GHOST_FLASH_TIMER_RESET 0.1
struct Ghost : Entity {
	int power_type;
	Vector2i direction;
	GhostState state;
	float death_timer;
	float flash_timer;
};

struct Window {
	Vector2i res;
	SDL_Window * sdl;
	uint32_t last_time;
	float delta_time = 0.0167;
};

static Window window;

struct Level {
	static const int play_w = 16;
	static const int play_h = 13;
	bool top_left = true;
	int grid[play_w * play_h];
	Vector2i crystal_pos;
	List<Entity> walls;
};

struct Game {
	const Level  * level;
	const Player * player;
	//const Ghost  * ghost;
	const List<Ghost> * ghosts;
};

static Game game;

inline int to_index(Vector2i pos, int w = Level::play_w)
{
	return pos.x + pos.y * w;
}

inline int to_index(int x, int y, int w = Level::play_w)
{
	return x + y * w;
}

void make_entity(Entity * e, Vector2i pos, Texture tex)
{
	e->pos = Vector2i(pos.x * 16, pos.y * 16);
	e->grid_pos = pos;
	e->target_pos = pos;
	e->move_t = 0;
	e->tex = tex;
	e->type = ENTITY;
}

void move_entity(Entity * e, Vector2i target)
{
	if (target.x == e->grid_pos.x && target.y == e->grid_pos.y) return;
	e->moving = true;
	e->move_t += window.delta_time;
	float t = e->move_t / 0.5;
	e->pos.x = (e->grid_pos.x * 16) + t * ((target.x - e->grid_pos.x) * 16);
	e->pos.y = (e->grid_pos.y * 16) + t * ((target.y - e->grid_pos.y) * 16);
	if (t >= 1) {
		e->move_t = 0;
		e->moving = false;
		e->grid_pos = target;
		e->pos.x = e->grid_pos.x * 16;
		e->pos.y = e->grid_pos.y * 16;
	}
}

void draw_entity(Entity * e)
{
	if (!e->visible) return;
	Render::render(e->pos, e->tex.pos, e->tex.dim, e->tex.scale);
}

void make_player(Player * p, Vector2i pos, int power_level)
{
	Texture tex;
	tex.pos = Vector2i(32 + (16 * power_level), 16);
	tex.dim = Vector2i(16, 16);
	tex.scale = Vector2f(1, 1);
	make_entity(p, pos, tex);
	p->power_level = power_level;
	p->power_max   = power_level;
	p->type = PLAYER;
}

void update_player(Player * p)
{
	p->tex.pos = Vector2i(48 + (16 * p->power_level), 16);
	if (!p->moving) {
		if (!game.level->grid[to_index(p->grid_pos + p->queued_direction)]) {
			p->direction = p->queued_direction;
		} else {
			p->direction = Vector2i(0, 0);
		}
	}
	move_entity(p, p->grid_pos + p->direction);
}

void keydown_player(Player * p, SDL_Scancode scancode)
{
	switch (scancode) {
	case SDL_SCANCODE_W: {
		p->queued_direction = Vector2i(+0, -1);
	} break;
	case SDL_SCANCODE_A: {
		p->queued_direction = Vector2i(-1, +0);
	} break;
	case SDL_SCANCODE_S: {
		p->queued_direction = Vector2i(+0, +1);
	} break;
	case SDL_SCANCODE_D: {
		p->queued_direction = Vector2i(+1, +0);
	} break;
	case SDL_SCANCODE_LEFT: {
		if (p->power_level > 0) {
			p->power_level--;
		}
	} break;
	case SDL_SCANCODE_RIGHT: {
		if (p->power_level < p->power_max) {
			p->power_level++;
		}
	} break;
	case SDL_SCANCODE_KP_6: {
		p->power_max++;
	} break;
	case SDL_SCANCODE_KP_4: {
		p->power_max--;
	} break;
	}
}

void make_ghost(Ghost * g, Vector2i pos, int power_type)
{
	Texture tex;
	tex.pos = Vector2i(48 + (16 * power_type), 32);
	tex.dim = Vector2i(16, 16);
	tex.scale = Vector2f(1, 1);
	make_entity(g, pos, tex);
	g->power_type = power_type;
	g->type = GHOST;
}

bool update_ghost(Ghost * g)
{
	if (g->state == GHOST_DEAD) {
		g->death_timer -= window.delta_time;
		g->flash_timer -= window.delta_time;
		if (g->flash_timer <= 0) {
			g->visible = !g->visible;
			g->flash_timer = GHOST_FLASH_TIMER_RESET;
		}
		return g->death_timer <= 0;
	}
	if (!g->moving) {
		List<Vector2i> dirs = a_star(
			game.level->grid, Level::play_w, Level::play_h,
			g->grid_pos, game.player->grid_pos);
		if (dirs.len == 0) {
			g->direction = Vector2i(0, 0);
			goto dealloc;
		}
		for (int i = 0; i < game.ghosts->len; i++) {
			if ((*game.ghosts)[i].state == GHOST_ALIVE &&
				(g->grid_pos + dirs[0]) == (*game.ghosts)[i].grid_pos) {
				g->direction = Vector2i(0, 0);
				goto dealloc;
			}
		}
		g->direction = dirs[0];
	dealloc:
		dirs.dealloc();
	}
	move_entity(g, g->grid_pos + g->direction);
	return false;
}

void kill_ghost(Ghost * g)
{
	if (g->state == GHOST_DEAD) return;
	g->state = GHOST_DEAD;
	// TODO(pixlark): Not very robust, if we want to make the ghost
	// alive again, then we have to set the texture position
	// again. But on the other hand, this time it's a one-time
	// operation rather than an extra if-statement every
	// frame. Probably doesn't matter either way but worth thinking
	// about.
	g->tex.pos.y = 48;
	g->death_timer = GHOST_DEATH_TIMER_RESET;
	g->flash_timer = GHOST_FLASH_TIMER_RESET;
}

Window make_window()
{
	float res_scale = 4.0;
	Window window;
	window.res = Vector2i(256, 240);
	window.sdl = SDL_CreateWindow("NES game",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		window.res.x * res_scale, window.res.y * res_scale,
		SDL_WINDOW_SHOWN|SDL_WINDOW_OPENGL);
	{
		List<char> exe_path = get_exe_dir();
		exe_path.cat("..\\atlas.png", 13, 1);
		printf("Loading atlas from %s\n", exe_path.arr);
	
		Render::init(window.sdl, exe_path.arr, window.res, res_scale);

		exe_path.dealloc();
	}
	window.last_time = SDL_GetPerformanceCounter();
	return window;
}

bool v2_equality(Vector2i a, Vector2i b) {
	return a.x == b.x && a.y == b.y;
}

void generate_level(Level * level)
{
	auto add_walls_to_list = [level](Vector2i w, List<Vector2i> * l) {
		for (int i = 0; i < 4; i++) {
			Vector2i n = w + directions[i];
			if (level->grid[to_index(n)]) {
				if (l->find(n, v2_equality) == -1 &&
					n.x >= 1 && n.x < Level::play_w - 1 && n.y >= 1 && n.y < Level::play_h - 1) {
					l->push(n);
				}
			}
		}
	};
	auto tunnel_from_point = [level, add_walls_to_list](Vector2i p) {
		List<Vector2i> walls;
		level->grid[to_index(p)] = 0;
		walls.alloc();
		add_walls_to_list(p, &walls);
		while (1) {
			if (walls.len == 0) break;
			int wi = rand() % walls.len;
			Vector2i w = walls[wi];
			int bordering = 0;
			for (int i = 0; i < 4; i++) {
				if (!level->grid[to_index(w + directions[i])]) {
					bordering++;
				}
			}
			if (bordering == 1) {
				level->grid[to_index(w)] = 0;
				add_walls_to_list(w, &walls);
			}
			walls.remove(wi);
		}
		walls.dealloc();
	};
	Vector2i start;
	if (level->top_left) {
		start.x = 1;
		start.y = 1;
		level->crystal_pos.x = Level::play_w - 2;
		level->crystal_pos.y = Level::play_h - 2;
	} else {
		start.x = Level::play_w - 2;
		start.y = Level::play_h - 2;
		level->crystal_pos.x = 1;
		level->crystal_pos.y = 1;
	}
	for (int i = 0; i < Level::play_w*Level::play_h; i++) level->grid[i] = 1;
	tunnel_from_point(start);
	{
		Vector2i dir = level->top_left ? Vector2i(-1, -1) : Vector2i(1, 1);
		while (level->grid[to_index(level->crystal_pos)]) {
			// TODO(pixlark): This has about a 1/2048 chance (I
			// think) of failing if the maze generation forms a
			// perfect diagonal line.
			level->crystal_pos += dir;
		}
	}
	// Turn level into entity list that can be rendered
	level->walls.dealloc();
	level->walls.alloc();
	for (int y = 0; y < Level::play_h; y++) {
		for (int x = 0; x < Level::play_w; x++) {
			if (level->grid[to_index(x, y)]) {
				Entity e;
				Texture t;
				t.pos = Vector2i(0, 5 * 16);
				t.dim = Vector2i(16, 16);
				t.scale = Vector2f(1, 1);
				make_entity(&e, Vector2i(x, y), t);
				level->walls.push(e);
			}
		}
	}
	level->top_left = !level->top_left;
}

Vector2i get_empty_level_spot(const Level * l)
{
	Vector2i pos;
	do {
		pos.x = rand() % Level::play_w;
		pos.y = rand() % Level::play_h;
	} while (l->grid[to_index(pos)]);
	return pos;
}

void draw_level(Level * l)
{
	for (int i = 0; i < l->walls.len; i++) {
		draw_entity(l->walls.arr + i);
	}
	Render::render(
		Vector2i(l->crystal_pos.x * 16, l->crystal_pos.y * 16),
		Vector2i((game.player->power_max + 1) * 16 + 48, 0), Vector2i(16, 16), Vector2f(1, 1));
}


int ghosts_per_level[] = {
	1, 2, 2, 3,
	3, 4, 4, 5,
};

void generate_ghosts(List<Ghost> * ghosts, int power)
{
	ghosts->dealloc();
	ghosts->alloc();
	for (int i = 0; i < ghosts_per_level[power]; i++) {
		Ghost g;
		make_ghost(&g, get_empty_level_spot(game.level), rand() % (power + 1));
		ghosts->push(g);
	}
}

void check_crystal(Level * level, Player * player, List<Ghost> * ghosts)
{
	if (player->grid_pos.x == level->crystal_pos.x &&
		player->grid_pos.y == level->crystal_pos.y) {
		// Player stuff
		player->power_max++;
		player->power_level = player->power_max;
		player->queued_direction = Vector2i(0, 0);
		// Ghost stuff
		generate_ghosts(ghosts, player->power_max);
		// Level generation
		generate_level(level);
	}
}

bool colliding(Entity * a, Entity * b, int buffer)
{
	Vector2i bf = Vector2i(buffer, buffer);
	Vector2i p0 = a->pos     + bf;
	Vector2i d0 = a->tex.dim - bf; 
	Vector2i p1 = b->pos     + bf;
	Vector2i d1 = b->tex.dim - bf;
	if (p0.x < p1.x + d1.x && p0.x + d0.x > p1.x &&
		p0.y < p1.y + d1.y && p0.y + d0.y > p1.y) {
		return true;
	}
	return false;
}

void check_collision(Player * player, List<Ghost> * ghosts)
{
	for (int i = ghosts->len - 1; i >= 0; i--) {
		if (colliding(player, ghosts->arr + i, 4)) {
			if (player->power_level == (*ghosts)[i].power_type) {
				kill_ghost(ghosts->arr + i);
			}
		}
	}
}

void update_level(Level * l)
{
	// Nothing for now...
}

void tick_delta_time()
{
	uint32_t new_time = SDL_GetPerformanceCounter();
	window.delta_time = (float) (new_time - window.last_time) / SDL_GetPerformanceFrequency();
	window.last_time = new_time;
	if (window.delta_time < 1.0 / 60.0) {
		SDL_Delay(((1.0 / 60.0) - window.delta_time) * 1000);
	}
}

int main()
{
	srand(time(NULL));
	
	SDL_Init(SDL_INIT_VIDEO);
	window = make_window();

	Player player;
	make_player(&player, Vector2i(1, 1), -1);
	game.player = &player;
	
	Level level;
	generate_level(&level);
	game.level = &level;

	List<Ghost> ghosts;
	ghosts.alloc();
	game.ghosts = &ghosts;
	
	SDL_Event event;
	bool running = true;
	while (running) {
		while (SDL_PollEvent(&event) != 0) {
			switch (event.type) {
			case SDL_QUIT:
				running = false;
				break;
			case SDL_KEYDOWN:
				keydown_player(&player, event.key.keysym.scancode);
				break;
			}
		}
		update_player(&player);
		for (int i = 0; i < ghosts.len; i++) {
			if (update_ghost(ghosts.arr + i)) {
				ghosts.remove(i);
			}
		}
		update_level (&level);
		check_crystal(&level, &player, &ghosts);
		check_collision(&player, &ghosts);
		Render::clear(RGBA(36, 56, 225, 255));
		draw_level   (&level);
		draw_entity  (&player);
		for (int i = 0; i < ghosts.len; i++) {
			draw_entity  (ghosts.arr + i);
		}
		
		{
			// Draw UI stuff
			// TODO(pixlark): Put this all in a UI struct or something
			for (int i = 0; i < 8; i++) {
				if (i == player.power_level) {
					Render::render(Vector2i(i * 32, window.res.y - 32), Vector2i(0, 48), Vector2i(32, 32), Vector2f(1, 1));
				} else {
					Render::render(Vector2i(i * 32, window.res.y - 32), Vector2i(0, 16), Vector2i(32, 32), Vector2f(1, 1));
				}
			}
			for (int i = 0; i < player.power_max + 1; i++) {
				Render::render(Vector2i(i * 32 + 8, window.res.y - 24), Vector2i(i * 16 + 48, 0), Vector2i(16, 16), Vector2f(1, 1));
			}
			Render::render(Vector2i(0, window.res.y - 48), Vector2i(0, 0), Vector2i(16, 16), Vector2f(16, 1));
		}
		Render::swap(window.sdl);
		tick_delta_time();
	}
	return 0;
}
