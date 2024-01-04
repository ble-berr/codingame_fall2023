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
#include <math.h>

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
#define NB_VECTOR_ANGLES (16)
#define NB_VECTOR_SPEEDS (2)

struct fish {
	int color;    /* [0,3] */
	int type;     /* [0,2] */
	int x;        /* [0, 10000[ */
	int y;        /* [0, 10000[ */
	int vx;       /* [0, 10000[ ? */
	int vy;       /* [0, 10000[ ? */
	bool visible;
	bool unavailable;
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
	int turns_since_light;
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
		drone->turns_since_light += 1;
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
	int x_diff = a.x - b.x;
	int y_diff = a.y - b.y;

	return sqrt((x_diff * x_diff) + (y_diff * y_diff));
}

#define MAX_FISH_VALUE (100000)

static int compute_fish_value(struct fish *fish) {
	int fish_id = ENTITY_ID(fish);

	int fish_value = fish->type + 1;

	int scanned_of_color = 0;
	int scanned_of_type = 0;
	for (int i = 0; i < state.my.scan_count; i++) {
		struct fish *scanned_fish = &state.entities[state.my.scans[i]].fish;
		if (scanned_fish->color == fish->color) { scanned_of_color += 1; }
		if (scanned_fish->type == fish->type) { scanned_of_type += 1; }
	}

	int color_value;
	switch (scanned_of_color) {
		case 0: color_value = 1; break;
		case 1: color_value = 1; break;
		case 2: color_value = 3; break;
	}

	int type_value;
	switch (scanned_of_type) {
		case 0: type_value = 1; break;
		case 1: type_value = 1; break;
		case 2: type_value = 2; break;
		case 3: type_value = 4; break;
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
	return fish_value * 100;
}

static bool monster_collision(struct drone *drone, struct vec2d vector) {
	for (int i = 0; i < COLLISION_POINTS_PER_VECTOR; i++) {
		struct vec2d drone_snapshot = {
			drone->x + ((vector.x * i) / COLLISION_POINTS_PER_VECTOR),
			drone->y + ((vector.y * i) / COLLISION_POINTS_PER_VECTOR),
		};

		for (int id = TOTAL_DRONE_COUNT; id < state.entity_count; id++) {
			struct fish *fish = &state.entities[id].fish;
			if (fish->type != -1) { continue; }

			struct vec2d monster_snapshot = {
				fish->x + ((fish->vx * i) / COLLISION_POINTS_PER_VECTOR),
				fish->y + ((fish->vy * i) / COLLISION_POINTS_PER_VECTOR),
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

static int compute_weighted_value(struct vec2d drone_pos, struct vec2d drone_vector, int fish_value, struct vec2d fish_pos) {
	int initial_distance = vec2d_distance(drone_pos, fish_pos);
	drone_vector.x += drone_pos.x;
	drone_vector.y += drone_pos.y;
	int final_distance = vec2d_distance(drone_vector, fish_pos);

	double factor = 1.0 - ((double)final_distance / (double)initial_distance);
	int weighted_value = (double)fish_value * factor;

	return weighted_value;
}

static struct vec2d movement_vectors[NB_VECTOR_ANGLES * NB_VECTOR_SPEEDS];

static void compute_movement_vectors(void) {
	for (int speed_i = 0; speed_i < NB_VECTOR_SPEEDS; speed_i++) {
		float speed = ceil((DRONE_TURN_MOVE_DISTANCE * (NB_VECTOR_SPEEDS - speed_i)) / (float)NB_VECTOR_SPEEDS);

		for (int rot_i = 0; rot_i < NB_VECTOR_ANGLES; rot_i++) {
			int vector_index = (speed_i * NB_VECTOR_ANGLES) + rot_i;
			float angle_rad = ((2 * rot_i) * M_PI) / NB_VECTOR_ANGLES;
			float rot_vx = speed * cos(angle_rad);
			float rot_vy = speed * sin(angle_rad);

			rot_vx = (rot_vx < 0) ? floorf(rot_vx) : ceilf(rot_vx);
			rot_vy = (rot_vy < 0) ? floorf(rot_vy) : ceilf(rot_vy);

			dbg(
				"vec#%d s:%f a:(%d/%d %fdeg %frad) v:{%f,%f}\n",
				vector_index, speed, rot_i, NB_VECTOR_ANGLES,
				(360.0 / (double)NB_VECTOR_ANGLES) * (double)rot_i, (double)angle_rad,
				rot_vx, rot_vy
			);

			movement_vectors[vector_index].x = (int)rot_vx;
			movement_vectors[vector_index].y = (int)rot_vy;
		}
	}

#if 0
	{
		char buf[256];
		int buflen = snprintf(buf, ARRLEN(buf), "mov vecs:");
		for (int i = 0; i < ARRLEN(movement_vectors); i++) {
			buflen += snprintf(buf + buflen, ARRLEN(buf) - buflen, " {%d,%d}", movement_vectors[i].x, movement_vectors[i].y);
		}
		dbg("%s\n", buf);
	}
#endif
}

static void play_drone(struct drone *drone) {
	const int light = (drone->y > 2000 && (drone->battery == DRONE_BATTERY_MAX || 4 <= drone->turns_since_light));
	if (light) {
		drone->turns_since_light = 0;
	} else {
		drone->turns_since_light += 1;
	}

	int other_drone_id = (state.my.drones[0] == ENTITY_ID(drone)) ? state.my.drones[1] : state.my.drones[0];
	struct drone *other_drone = &state.entities[other_drone_id].drone;

	struct vec2d drone_pos = { drone->x, drone->y };
	struct vec2d other_drone_pos = { other_drone->x, other_drone->y };

	int vector_count = 0;
	struct vec2d vectors[ARRLEN(movement_vectors)];
	int vector_fish_scores[ARRLEN(movement_vectors)] = {};
	int vector_scan_scores[ARRLEN(movement_vectors)] = {};
	int vector_drone_scores[ARRLEN(movement_vectors)] = {};

	for (int i = 0; i < ARRLEN(movement_vectors); i++) {
		if (!monster_collision(drone, movement_vectors[i])) {
			vectors[vector_count] = movement_vectors[i];
			vector_count += 1;
		}
	}

	if (!vector_count) {
		submit_drone_wait(light, "trapped!");
		return;
	}

	for (int ent_id = TOTAL_DRONE_COUNT; ent_id < state.entity_count; ent_id++) {
		struct fish *fish = &state.entities[ent_id].fish;
		if (fish->type == -1 || fish->unavailable || is_scanned(drone, ent_id)) { continue; }

		int fish_value = compute_fish_value(fish);

		struct vec2d fish_pos = { fish->x + fish->vx, fish->y + fish->vy };

		if (is_scanned(other_drone, ent_id) || vec2d_distance(other_drone_pos, fish_pos) < vec2d_distance(drone_pos, fish_pos)) {
			fish_value /= 2;
		}

		for (int i = 0; i < vector_count; i++) {
			if (fish_will_scan(drone, vectors[i], fish)) {
				vector_fish_scores[i] += fish_value;
			} else {
				vector_fish_scores[i] += compute_weighted_value(drone_pos, vectors[i], fish_value, fish_pos);
			}
		}
	}

	int drone_scans_value = 0;
	for (int i = 0; i < drone->scan_count; i++) {
		struct fish *fish = &state.entities[drone->scans[i]].fish;
		drone_scans_value += compute_fish_value(fish);
	}

	for (int i = 0; i < vector_count; i++) {
		if (drone_pos.y <= DRONE_SCAN_SUBMIT_DEPTH) { continue; }
		int final_y = drone_pos.y + vectors[i].y;
		if (final_y <= DRONE_SCAN_SUBMIT_DEPTH) { vector_scan_scores[i] += drone_scans_value; }

		int initial_distance = drone_pos.y - DRONE_SCAN_SUBMIT_DEPTH;
		int final_distance = final_y - DRONE_SCAN_SUBMIT_DEPTH;

		double factor = 1.0 - ((double)final_distance / (double)initial_distance);
		vector_scan_scores[i] += (int)((double)drone_scans_value * factor);
	}

	for (int i = 0; i < vector_count; i++) {
		vector_drone_scores[i] += compute_weighted_value(drone_pos, vectors[i], -1, other_drone_pos);
	}

#if 1
	{
		char buf[256];
		int buf_len;

		buf_len = snprintf(buf, ARRLEN(buf), "D%ld", ENTITY_ID(drone));
		for (int i = 0; i < vector_count; i++) {
			buf_len += snprintf(buf + buf_len, ARRLEN(buf) - buf_len, " {%d,%d}:[%d,%d,%d]%d",
					vectors[i].x, vectors[i].y,
					vector_fish_scores[i], vector_scan_scores[i], vector_drone_scores[i],
					vector_fish_scores[i] + vector_scan_scores[i] + vector_drone_scores[i]
			);
		}
		dbg("%s\n", buf);
	}
#endif

	int best_score = vector_fish_scores[0] + vector_scan_scores[0] + vector_drone_scores[0];
	int best_vector = 0;
	for (int i = 1; i < vector_count; i++) {
		int score = vector_fish_scores[i] + vector_scan_scores[i] + vector_drone_scores[i];
		if (best_score < score) {
			best_score = score;
			best_vector = i;
		}
	}

	dbg("best is {%d,%d}\n", vectors[best_vector].x, vectors[best_vector].y);

	drone_pos.x += vectors[best_vector].x;
	drone_pos.y += vectors[best_vector].y;

	submit_drone_move(drone_pos.x, drone_pos.y, light, "");
}

static void guess_fish_positions(void) {
	struct drone *drone_a = &state.entities[state.my.drones[0]].drone;
	struct drone *drone_b = &state.entities[state.my.drones[1]].drone;
	assert(drone_a->blip_count == drone_b->blip_count, "blip count mismatch\n");

	for (int fish_id = TOTAL_DRONE_COUNT; fish_id < state.entity_count; fish_id++) {
		struct fish *fish = &state.entities[fish_id].fish;
		if (fish->type == -1 || fish->visible) { continue; }

		enum direction *dir_a = NULL;
		enum direction *dir_b = NULL;

		for (int i = 0; i < drone_a->blip_count; i++) {
			if (drone_a->blips[i].creature_id == fish_id) { dir_a = &drone_a->blips[i].direction; }
			if (drone_b->blips[i].creature_id == fish_id) { dir_b = &drone_b->blips[i].direction; }
		}
		assert((!dir_a && !dir_b) || (dir_a && dir_b), "blip mismatch\n");
		if (!dir_a) {
			fish->unavailable = true;
			continue;
		}

		int const top_hab_lim[] = { 2500, 5000, 7500 };
		int const bot_hab_lim[] = { 5000, 7500, 10000 };
		int left_x = 0;
		int right_x = MAX_X;
		int top_y = top_hab_lim[fish->type];
		int bottom_y = bot_hab_lim[fish->type];

		switch (*dir_a) {
			case BL: right_x = MIN(right_x, drone_a->x); top_y    = MAX(top_y,    drone_a->y); break;
			case TL: right_x = MIN(right_x, drone_a->x); bottom_y = MAX(bottom_y, drone_a->y); break;
			case BR: left_x  = MIN(left_x,  drone_a->x); top_y    = MAX(top_y,    drone_a->y); break;
			case TR: left_x  = MIN(left_x,  drone_a->x); bottom_y = MAX(bottom_y, drone_a->y); break;
		}

		switch (*dir_b) {
			case BL: right_x = MIN(right_x, drone_b->x); top_y    = MAX(top_y,    drone_b->y); break;
			case TL: right_x = MIN(right_x, drone_b->x); bottom_y = MAX(bottom_y, drone_b->y); break;
			case BR: left_x  = MIN(left_x,  drone_b->x); top_y    = MAX(top_y,    drone_b->y); break;
			case TR: left_x  = MIN(left_x,  drone_b->x); bottom_y = MAX(bottom_y, drone_b->y); break;
		}

		fish->x = (left_x + right_x) / 2;
		fish->y = (top_y + bottom_y) / 2;
		fish->vx = 0;
		fish->vy = 0;
	}
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

	compute_movement_vectors();

	while (1) {
		parse_round_input();
		guess_fish_positions();

		play_drone(&state.entities[state.my.drones[0]].drone);
		play_drone(&state.entities[state.my.drones[1]].drone);
	}

	return 0;
}
