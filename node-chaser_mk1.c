/*
 * Bronze League
 *
 * Goes through a preset route
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#define ARRLEN(x) (sizeof(x) / sizeof(*x))

static void assert(bool cond, char *fmt, ...) {
	if (!cond) {
		va_list args;
		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
		abort();
	}
}

#if 0
#define DEBUG_BUILD
#endif

static void dbg(char *fmt, ...) {
#ifdef DEBUG_BUILD
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
#endif
}

#define MAX_X (10000)
#define MAX_Y (10000)

#define DRONE_BATTERY_MAX (30)

#define DRONE_FISH_SCAN_DIST (800)
#define DRONE_TURN_MOVE_DIST (600)
#define DRONE_SCAN_SUBMIT_DEPTH (500)

#define PLAYER_COUNT (2)
#define PLAYER_DRONE_COUNT (2)
#define TOTAL_DRONE_COUNT (PLAYER_DRONE_COUNT * PLAYER_COUNT)

#define FISH_COLOR_COUNT (4)
#define FISH_TYPE_COUNT (3)
#define FISH_COUNT (FISH_COLOR_COUNT * FISH_TYPE_COUNT)

/* MIN is 1 */
#define MONSTER_COUNT_MAX (8)

/* 4 drones and 12 fish in Wood League 1. */
#define MAX_ENTITIES (TOTAL_DRONE_COUNT + FISH_COUNT + MONSTER_COUNT_MAX)

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

		enum direction direction = (radar[0] == 'T') | ((radar[1] == 'R') << 1);

		struct drone *drone = &state.entities[drone_id].drone;
		drone->blips[drone->blip_count].creature_id = creature_id;
		drone->blips[drone->blip_count].direction = direction;
		drone->blip_count += 1;
	}
}

static void parse_round_input_old(void) {
	scanf("%d%d%d", &state.my.score, &state.foe.score, &state.my.scan_count);
	assert(
		0 <= state.my.scan_count && state.my.scan_count <= ARRLEN(state.my.scans),
		"Unexpected scan count (my): %d\n", state.my.scan_count
	);

	for (int i = 0; i < state.my.scan_count; i++) {
		scanf("%d", &state.my.scans[i]);
	}

	scanf("%d", &state.foe.scan_count);
	assert(
		0 <= state.foe.scan_count && state.foe.scan_count <= ARRLEN(state.foe.scans),
		"Unexpected scan count (foe): %d\n", state.foe.scan_count
	);

	for (int i = 0; i < state.foe.scan_count; i++) {
		scanf("%d", &state.foe.scans[i]);
	}

	scanf("%d", &state.my.drone_count);
	assert(state.my.drone_count == PLAYER_DRONE_COUNT, "Unexpected number of drones: %d\n", state.my.drone_count);

	for (int i = 0; i < state.my.drone_count; i++) {
		scanf("%d", &state.my.drones[i]);

		struct drone *drone = &state.entities[state.my.drones[i]].drone;
		scanf("%d%d%d%d", &drone->x, &drone->y, &drone->emergency, &drone->battery);
		drone->blip_count = 0;
		drone->scan_count = 0;
	}

	scanf("%d", &state.foe.drone_count);
	assert(state.foe.drone_count == PLAYER_DRONE_COUNT, "Unexpected number of drones: %d\n", state.foe.drone_count);

	for (int i = 0; i < state.foe.drone_count; i++) {
		scanf("%d", &state.foe.drones[i]);

		struct drone *drone = &state.entities[state.foe.drones[i]].drone;
		scanf("%d%d%d%d", &drone->x, &drone->y, &drone->emergency, &drone->battery);
		drone->blip_count = 0;
		drone->scan_count = 0;
	}

	int drone_scan_count;
	scanf("%d", &drone_scan_count);

	for (int i = 0; i < drone_scan_count; i++) {
		int drone_id;
		int creature_id;
		scanf("%d%d", &drone_id, &creature_id);

		struct drone *drone = &state.entities[drone_id].drone;
		assert(drone->scan_count < ARRLEN(drone->scans), "Drone scan overflow\n");
		drone->scans[drone->scan_count] = creature_id;
		drone->scan_count += 1;
	}

	int visible_creature_count;
	scanf("%d", &visible_creature_count);

	for (int i = 0; i < visible_creature_count; i++) {
		int id;
		scanf("%d", &id);

		struct fish *fish = &state.entities[id].fish;
		scanf("%d%d%d%d", &fish->x, &fish->y, &fish->vx, &fish->vy);
		fish->visible = true;
	}

	int radar_blip_count;
	scanf("%d", &radar_blip_count);

	for (int i = 0; i < radar_blip_count; i++) {
		int drone_id;
		int creature_id;
		char radar[3];
		scanf("%d%d%s", &drone_id, &creature_id, radar);

		struct drone *drone = &state.entities[drone_id].drone;
		assert(drone->blip_count < ARRLEN(drone->blips), "Drone blip overflow\n");
		drone->blips[drone->blip_count].creature_id = creature_id;
		drone->blips[drone->blip_count].direction = (radar[0] == 'T') | ((radar[1] == 'R') << 1);
		drone->blip_count += 1;
	}
}

static int abs_dist(int ax, int ay, int bx, int by) {
	int dx = ax - bx;
	int dy = ay - by;

	return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

static bool is_scanned(int fish_id) {
	for (int i = 0; i < ARRLEN(state.my.scans); i++) {
		if (state.my.scans[i] == fish_id) { return true; }
	}

	for (int i = 0; i < PLAYER_DRONE_COUNT; i++) {
		struct drone *drone = &state.entities[state.my.drones[i]].drone;

		for (int j = 0; j < drone->scan_count; j++) {
			if (drone->scans[j] == fish_id) { return true; }
		}
	}

	return false;
}

static int fish_value_heuristic(int fish_id) {
	return !is_scanned(fish_id);
}

static void play_drone(struct drone *drone, bool inverse_priority) {
	const int light = (drone->battery == DRONE_BATTERY_MAX);

	if (drone->emergency) {
		drone->state = EMERGENCY;
		submit_drone_move(drone->route_start_x, drone->route_start_y, light, "emergency!");
		return;
	}

	switch (drone->state) {
		case EMERGENCY:
			drone->state = STARTING_ROUTE;
			drone->route_step = 0;
			drone->route_start_x = (drone->x < 5000) ? 2500 : 7500;
			drone->route_start_y = 2500;
			break;
		case STARTING_ROUTE:
			if (drone->x == drone->route_start_x && drone->y == drone->route_start_y) {
				drone->state = ROUTING_DOWN;
			}
			break;
		case ROUTING_DOWN:
			if ((MAX_Y - drone->y) <= (DRONE_FISH_SCAN_DIST / 2)) {
				drone->state = ROUTING_UP;
			}
			break;
		case ROUTING_UP:
			if (2500 <= drone->y) {
				drone->state = SURFACING;
			}
			break;
		case SURFACING:
			if (drone->y <= DRONE_SCAN_SUBMIT_DEPTH) {
				drone->state = STARTING_ROUTE;
				drone->route_step = 0;
				drone->route_start_x = (drone->x < 5000) ? 2500 : 7500;
				drone->route_start_y = 2500;
			}
			break;
	}

	static int const route_dx[] = {
		600, 500, 400, 300, 200, 100,
		0, -100, -200, -300, -400, -500,
		-600, -500, -400, -300, -200, -100,
		0, 100, 200, 300, 400, 500,
	};

	static int const route_dy[] = {
		0, 100, 200, 300, 400, 500,
		600, 500, 400, 300, 200, 100,
	};

	switch (drone->state) {
		case EMERGENCY:
			assert(false, "unreachable statement: %d", __LINE__);
			return;
		case STARTING_ROUTE:
			submit_drone_move(drone->route_start_x, drone->route_start_y, light, "starting route");
			return;
		case ROUTING_DOWN:
			{
				int dx = route_dx[drone->route_step % ARRLEN(route_dx)];
				int dy = route_dy[drone->route_step % ARRLEN(route_dy)];
				submit_drone_move(drone->x + dx, drone->y + dy, light, "routing down");
				drone->route_step += 1;
			}
			return;
		case ROUTING_UP:
			{
				int dx = route_dx[drone->route_step % ARRLEN(route_dx)];
				int dy = route_dy[drone->route_step % ARRLEN(route_dy)] * -1;
				submit_drone_move(drone->x + dx, drone->y + dy, light, "routing up");
				drone->route_step += 1;
			}
			return;
		case SURFACING:
			submit_drone_move(drone->x, DRONE_SCAN_SUBMIT_DEPTH, light, "surfacing");
			return;
	}
}

int main()
{
	int creature_count;
	scanf("%d", &creature_count);

	for (int i = 0; i < creature_count; i++) {
		int id;
		scanf("%d", &id);

		struct fish *fish = &state.entities[id].fish;
		scanf("%d%d", &fish->color, &fish->type);
	}

	int type = 0;

	while (1) {
		parse_round_input();

		play_drone(&state.entities[state.my.drones[0]].drone, false);
		play_drone(&state.entities[state.my.drones[1]].drone, true);
	}

	return 0;
}
