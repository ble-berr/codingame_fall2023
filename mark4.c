/*
 * Bronze League
 *
 * Selects the "best" direction for each drone from a fixed set of possible directions.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>

#define ARRLEN(x) (sizeof(x) / sizeof(*x))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static void assert(bool cond, char *fmt, ...) {
	if (!cond) {
		va_list args;
		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
		abort();
	}
}

#if 1
#define DEBUG_BUILD
#endif

static void dbg(char *fmt, ...) {
	(void)fmt;
#ifdef DEBUG_BUILD
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
#endif
}

struct vec2d {
	int x;
	int y;
};

#define MAX_X (10000)
#define MAX_Y (10000)

#define DRONE_BATTERY_MAX (30)

#define DRONE_FISH_SCAN_DISTANCE (800)
#define DRONE_TURN_MOVE_DISTANCE (600)
#define DRONE_SCAN_SUBMIT_DEPTH (500)

#define PLAYER_COUNT (2)
#define PLAYER_DRONE_COUNT (2)
#define TOTAL_DRONE_COUNT (PLAYER_DRONE_COUNT * PLAYER_COUNT)

#define FISH_COLOR_COUNT (4)
#define FISH_TYPE_COUNT (3)
#define FISH_COUNT (FISH_COLOR_COUNT * FISH_TYPE_COUNT)

#define MONSTER_COUNT_MAX (8) /* MIN is 1 */
#define MONSTER_COLLISION_DISTANCE (500)

/* 4 drones and 12 fish in Wood League 1. */
#define MAX_ENTITIES (TOTAL_DRONE_COUNT + FISH_COUNT + MONSTER_COUNT_MAX)

#define COLLISION_POINTS_PER_VECTOR (10)

struct fish {
	int color;    /* [0,3] */
	int type;     /* [0,2] */
	int x;        /* [0, 10000[ */
	int y;        /* [0, 10000[ */
	int vx;       /* [0, 10000[ ? */
	int vy;       /* [0, 10000[ ? */
	bool visible;
};

enum direction {
	BL,
	TL,
	BR,
	TR,
};

struct radar_blip {
	int creature_id;
	enum direction direction;
};

enum drone_state {
	EMERGENCY,
	STARTING_ROUTE,
	ROUTING_DOWN,
	ROUTING_UP,
	SURFACING,
};

struct drone {
	int x;         /* [0, 10000] */
	int y;         /* [0, 10000] */
	int emergency; /* ??? */
	int battery;   /* [0, 30] */
	int blip_count;
	struct radar_blip blips[FISH_COUNT + MONSTER_COUNT_MAX];
	int scan_count;
	int scans[FISH_COUNT];
	enum drone_state state;
	int route_step;
	int route_start_x;
	int route_start_y;
};

union entity {
	struct fish fish;
	struct drone drone;
};

struct player_state {
	int score;
	int scan_count;
	/* 3 types, 4 colors */
	int scans[FISH_COUNT];
	int drone_count;
	int drones[PLAYER_DRONE_COUNT];
};

struct state {
	struct player_state my;
	struct player_state foe;
	int entity_count;
	union entity entities[MAX_ENTITIES];
};

static struct state state;
#define ENTITY_ID(ptr) ((union entity*)ptr - state.entities)

static void submit_drone_move(int x, int y, int light, char *dbg, ...) {
#ifndef DEBUG_BUILD
	(void)dbg;
	printf("MOVE %d %d %d\n", x, y, light);
	return;
#else
	{
		char buf[128];
		va_list varargs;

		va_start(varargs, dbg);
		vsnprintf(buf, ARRLEN(buf), dbg, varargs);
		va_end(varargs);

		printf("MOVE %d %d %d %s\n", x, y, light, buf);
	}
	return;
#endif
}

static void submit_drone_wait(int light, char *dbg, ...) {
#ifdef DEBUG_BUILD
	(void)dbg;
	printf("WAIT %d\n", light);
	return;
#else
	{
		char buf[128];
		va_list varargs;

		va_start(varargs, dbg);
		vsnprintf(buf, ARRLEN(buf), dbg, varargs);
		va_end(varargs);

		printf("WAIT %d %s\n", light, buf);
	}
	return;
#endif
}

static void parse_round_input(void) {
	scanf("%d", &state.my.score);
	scanf("%d", &state.foe.score);
	scanf("%d", &state.my.scan_count);
	for (int i = 0; i < state.my.scan_count; i++) {
		int creature_id;
		scanf("%d", &creature_id);
		state.my.scans[i] = creature_id;
	}
	scanf("%d", &state.foe.scan_count);
	for (int i = 0; i < state.foe.scan_count; i++) {
		int creature_id;
		scanf("%d", &creature_id);
		state.foe.scans[i] = creature_id;
	}
	scanf("%d", &state.my.drone_count);
	for (int i = 0; i < state.my.drone_count; i++) {
		int drone_id;
		int drone_x;
		int drone_y;
		int emergency;
		int battery;
		scanf("%d%d%d%d%d", &drone_id, &drone_x, &drone_y, &emergency, &battery);

		struct drone *drone = &state.entities[drone_id].drone;
		state.my.drones[i] = drone_id;
		drone->x = drone_x;
		drone->y = drone_y;
		drone->emergency = emergency;
		drone->battery = battery;
		drone->scan_count = 0;
		drone->blip_count = 0;
	}
	scanf("%d", &state.foe.drone_count);
	for (int i = 0; i < state.foe.drone_count; i++) {
		int drone_id;
		int drone_x;
		int drone_y;
		int emergency;
		int battery;
		scanf("%d%d%d%d%d", &drone_id, &drone_x, &drone_y, &emergency, &battery);

		struct drone *drone = &state.entities[drone_id].drone;
		state.foe.drones[i] = drone_id;
		drone->x = drone_x;
		drone->y = drone_y;
		drone->emergency = emergency;
		drone->battery = battery;
		drone->scan_count = 0;
		drone->blip_count = 0;
	}
	int drone_scan_count;
	scanf("%d", &drone_scan_count);
	for (int i = 0; i < drone_scan_count; i++) {
		int drone_id;
		int creature_id;
		scanf("%d%d", &drone_id, &creature_id);

		struct drone *drone = &state.entities[drone_id].drone;
		drone->scans[drone->scan_count++] = creature_id;
	}
	int visible_creature_count;
	scanf("%d", &visible_creature_count);
	for (int i = 0; i < visible_creature_count; i++) {
		int creature_id;
		int creature_x;
		int creature_y;
		int creature_vx;
		int creature_vy;
		scanf("%d%d%d%d%d", &creature_id, &creature_x, &creature_y, &creature_vx, &creature_vy);

		struct fish *fish = &state.entities[creature_id].fish;
		fish->x = creature_x;
		fish->y = creature_y;
		fish->vx = creature_vx;
		fish->vy = creature_vy;
	}
	int radar_blip_count;
	scanf("%d", &radar_blip_count);
	for (int i = 0; i < radar_blip_count; i++) {
		int drone_id;
		int creature_id;
		char radar[3];
		scanf("%d%d%s", &drone_id, &creature_id, radar);

		struct drone *drone = &state.entities[drone_id].drone;
		drone->blips[drone->blip_count].creature_id = creature_id;
		if (radar[0] == 'B' && radar[1] == 'L') { drone->blips[drone->blip_count].direction = BL; }
		else if (radar[0] == 'T' && radar[1] == 'L') { drone->blips[drone->blip_count].direction = TL; }
		else if (radar[0] == 'B' && radar[1] == 'R') { drone->blips[drone->blip_count].direction = BR; }
		else if (radar[0] == 'T' && radar[1] == 'R') { drone->blips[drone->blip_count].direction = TR; }
		else { assert(false, "unkown direction: %s\n", radar); }
		drone->blip_count += 1;
	}
}

static int abs_dist(int ax, int ay, int bx, int by) {
	int dx = ax - bx;
	int dy = ay - by;

	return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

static bool is_scanned(struct drone *drone, int fish_id) {
	for (int i = 0; i < ARRLEN(state.my.scans); i++) {
		if (state.my.scans[i] == fish_id) { return true; }
	}

	for (int j = 0; j < drone->scan_count; j++) {
		if (drone->scans[j] == fish_id) { return true; }
	}

	return false;
}

static int vec2d_distance(struct vec2d a, struct vec2d b) {
	return ((a.x < b.x) ? (b.x - a.x) : (a.x - b.x))
		+ ((a.y < b.y) ? (b.y - a.y) : (a.y - b.y));
}

#define MAX_FISH_VALUE (100000)

static int compute_fish_value(struct fish *fish) {
	int fish_id = ENTITY_ID(fish);

	int fish_value = fish->type + 1;
	int color_value = 1;
	int type_value = 1;

	for (int i = 0; i < state.my.scan_count; i++) {
		struct fish *scanned_fish = &state.entities[state.my.scans[i]].fish;
		if (scanned_fish->color == fish->color) { color_value += 1; }
		if (scanned_fish->type == fish->type) { type_value += 1; }
	}

	bool foe_has_fish = false;
	int foe_color_count = 0;
	int foe_type_count = 0;

	for (int i = 0; i < state.foe.scan_count; i++) {
		if (state.foe.scans[i] == fish_id) { foe_has_fish = true; }
		struct fish *scanned_fish = &state.entities[state.my.scans[i]].fish;
		if (scanned_fish->color == fish->color) { foe_color_count += 1; }
		if (scanned_fish->type == fish->type) { foe_type_count += 1; }
	}

	if (!foe_has_fish) { fish_value *= 2; }
	if (foe_color_count < 3) { color_value *= 2; }
	if (foe_type_count < 4) { type_value *= 2; }

	int color_blip_count = 0;
	int type_blip_count = 0;

	struct drone *drone = &state.entities[state.my.drones[0]].drone;

	for (int i = 0; i < drone->blip_count; i++) {
		struct fish *radar_fish = &state.entities[drone->blips[i].creature_id].fish;
		if (radar_fish->color == fish->color) { color_blip_count += 1; }
		if (radar_fish->type == fish->type) { type_blip_count += 1; }
	}

	if (color_blip_count < 3) { color_value = 0; }
	if (type_blip_count < 4) { type_value = 0; }

	fish_value += color_value + type_value;

	assert(fish_value <= MAX_FISH_VALUE, "fish value too high!\n");
	return fish_value;
}

static bool monster_collision(struct drone *drone, struct vec2d vector) {
	vector.x /= COLLISION_POINTS_PER_VECTOR;
	vector.y /= COLLISION_POINTS_PER_VECTOR;

	for (int i = 0; i < COLLISION_POINTS_PER_VECTOR; i++) {
		struct vec2d drone_snapshot = { drone->x + (vector.x * i), drone->y + (vector.y * i) };

		for (int id = TOTAL_DRONE_COUNT; id < state.entity_count; id++) {
			struct fish *fish = &state.entities[id].fish;
			if (fish->type != -1) { continue; }

			struct vec2d monster_snapshot = {
				fish->x + ((fish->vx / COLLISION_POINTS_PER_VECTOR) * i),
				fish->y + ((fish->vy / COLLISION_POINTS_PER_VECTOR) * i),
			};

			if (vec2d_distance(drone_snapshot, monster_snapshot) <= MONSTER_COLLISION_DISTANCE) {
				return true;
			}
		}
	}

	return false;
}

static bool fish_will_scan(struct drone *drone, struct vec2d drone_vec, struct fish *fish) {
	drone_vec.x /= COLLISION_POINTS_PER_VECTOR;
	drone_vec.y /= COLLISION_POINTS_PER_VECTOR;

	for (int i = 0; i < COLLISION_POINTS_PER_VECTOR; i++) {
		struct vec2d drone_snapshot = { drone->x + (drone_vec.x * i), drone->y + (drone_vec.y * i) };

		struct vec2d fish_snapshot = {
			fish->x + ((fish->vx / COLLISION_POINTS_PER_VECTOR) * i),
			fish->y + ((fish->vy / COLLISION_POINTS_PER_VECTOR) * i),
		};

		if (vec2d_distance(drone_snapshot, fish_snapshot) <= DRONE_FISH_SCAN_DISTANCE) {
			return true;
		}
	}

	return false;
}

static struct vec2d fish_pos_from_radar(struct drone *drone, struct fish *fish) {
	struct vec2d fish_pos;

	int top;
	int bottom;

	switch (fish->type) {
		case 0:
			top = 2500;
			bottom = 5000;
			break;
		case 1:
			top = 5000;
			bottom = 7500;
			break;
		case 2:
			top = 7500;
			bottom = 10000;
			break;
		default: assert(false, "unreachable statement at line %d\n", __LINE__);
	}

	for (int i = 0; i < drone->blip_count; i++) {
		if (drone->blips[i].creature_id != ENTITY_ID(fish)) { continue; }
		switch (drone->blips[i].direction) {
			case BR:
				fish_pos.x = 9999;
				fish_pos.y = bottom - ((bottom - MAX(drone->y, top)) / 2);
				break;
			case TR:
				fish_pos.x = 9999;
				fish_pos.y = top + ((top - MIN(drone->y, bottom)) / 2);
				break;
			case BL:
				fish_pos.x = 0;
				fish_pos.y = bottom - ((bottom - MAX(drone->y, top)) / 2);
				break;
			case TL:
				fish_pos.x = 0;
				fish_pos.y = top + ((top - MIN(drone->y, bottom)) / 2);
				break;
			default: assert(false, "unreachable statement at line %d\n", __LINE__);
		}
		break;
	}

	return fish_pos;
}

static void play_drone(struct drone *drone) {
	const int light = (drone->battery == DRONE_BATTERY_MAX);

	struct vec2d candidate_vectors[] = {
		{ 600, 0 },
		{ 300, 300},
		{ 0, 600 },
		{ -300, 300 },
		{ -600, 0 },
		{ -300, -300 },
		{ 0, -600 },
		{ 300, -300 },
	};

	int vector_count = 0;
	struct vec2d vectors[ARRLEN(candidate_vectors)];
	int vector_scores[ARRLEN(candidate_vectors)] = {};

	for (int i = 0; i < ARRLEN(candidate_vectors); i++) {
		if (!monster_collision(drone, candidate_vectors[i])) {
			vectors[vector_count] = candidate_vectors[i];
			vector_count += 1;
		}
	}
	dbg("drone %d vector count %d\n", ENTITY_ID(drone), vector_count);

	if (!vector_count) {
		submit_drone_wait(light, "trapped!");
		return;
	}

	for (int i = TOTAL_DRONE_COUNT; i < state.entity_count; i++) {
		struct fish *fish = &state.entities[i].fish;
		if (fish->type == -1 || is_scanned(drone, i)) { continue; }

		int fish_value = compute_fish_value(fish);

		int distances[ARRLEN(candidate_vectors)];
		for (int j = 0; j < vector_count; j++) {
			struct vec2d *vec = vectors + j;
			if (fish_will_scan(drone, *vec, fish)) {
				distances[j] = 0;
				continue;
			}

			struct vec2d drone_pos = { drone->x + vec->x, drone->y + vec->y };
			struct vec2d fish_pos;

			if (fish->visible) {
				fish_pos.x = fish->x + fish->vx;
				fish_pos.y = fish->y + fish->vy;
			} else {
				fish_pos = fish_pos_from_radar(drone, fish);
			}

			distances[j] = vec2d_distance(drone_pos, fish_pos);
		}

		int penalties[ARRLEN(candidate_vectors)] = {};
		for (int j = 0; j < vector_count; j++) {
			for (int k = 0; k < vector_count; k++) {
				if (distances[j] < distances[k]) { continue; }
				penalties[j] += 1;
			}
		}

		for (int j = 0; j < vector_count; j++) {
			vector_scores[j] += fish_value / penalties[j];
		}
	}

	int drone_scans_value = 0;
	for (int i = 0; i < drone->scan_count; i++) {
		struct fish *fish = &state.entities[drone->scans[i]].fish;
		drone_scans_value += compute_fish_value(fish);
	}

	for (int i = 0; i < vector_count; i++) {
		float factor = (float)vectors[i].y / (float)-600;
		vector_scores[i] += (int)((float)drone_scans_value * factor);
	}

#ifdef DEBUG_BUILD
	{
		char buf[256] = { '{' };
		int buf_len = 1;
		for (int i = 0; i < vector_count; i++) {
			int len = snprintf(buf + buf_len, ARRLEN(buf) - buf_len, "%d,%d: %d", vectors[i].x, vectors[i].y, vector_scores[i]);
			assert(0 < len && len <= ARRLEN(buf) - buf_len, "buffer overflow line %d\n", __LINE__);
			buf_len += len;
			if (i < vector_count) {
				assert(buf_len < ARRLEN(buf) - 2, "buffer overflow line %d\n", __LINE__);
				buf[buf_len] = ',';
				buf[buf_len + 1] = ' ';
				buf_len += 2;
			}
		}
		assert(buf_len < ARRLEN(buf) - 2, "buffer overflow line %d\n", __LINE__);
		buf[buf_len] = '}';
		buf[buf_len + 1] = 0;
		dbg("D%d %s\n", ENTITY_ID(drone), buf);
	}
#endif

	int best_vector_score = vector_scores[0];
	int best_vector = 0;
	for (int i = 1; i < vector_count; i++) {
		if (best_vector_score < vector_scores[i]) {
			best_vector_score = vector_scores[i];
			best_vector = i;
		}
	}

	struct vec2d drone_pos = { drone->x, drone->y };
	drone_pos.x += vectors[best_vector].x;
	drone_pos.y += vectors[best_vector].y;

	submit_drone_move(drone_pos.x, drone_pos.y, light, "");
}

int main()
{
	int creature_count;
	scanf("%d", &creature_count);

	state.entity_count = TOTAL_DRONE_COUNT + creature_count;

	for (int i = 0; i < creature_count; i++) {
		int id;
		scanf("%d", &id);

		struct fish *fish = &state.entities[id].fish;
		scanf("%d%d", &fish->color, &fish->type);
	}

	while (1) {
		parse_round_input();

		play_drone(&state.entities[state.my.drones[0]].drone);
		play_drone(&state.entities[state.my.drones[1]].drone);
	}

	return 0;
}
