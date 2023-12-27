/*
 * Wood League 1
 *
 * WIP
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

static void dbg(char *fmt, ...) {
#if 0
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
#endif
}

#define MAX_X (10000)
#define MAX_Y (10000)

#define DRONE_BATTERY_MAX (30)

#define PLAYER_COUNT (2)
#define PLAYER_DRONE_COUNT (2)
#define TOTAL_DRONE_COUNT (PLAYER_DRONE_COUNT * PLAYER_COUNT)

#define FISH_COLOR_COUNT (4)
#define FISH_TYPE_COUNT (3)
#define FISH_COUNT (FISH_COLOR_COUNT * FISH_TYPE_COUNT)

/* 4 drones and 12 fish in Wood League 1. */
#define MAX_ENTITIES (TOTAL_DRONE_COUNT + FISH_COUNT)

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
	NO_DIRECTION,
};

struct radar_blip {
	int creature_id;
	enum direction direction;
};

struct drone {
	int x;         /* [0, 10000] */
	int y;         /* [0, 10000] */
	int emergency; /* ??? */
	int battery;   /* [0, 30] */
	int blip_count;
	struct radar_blip blips[FISH_COUNT];
	int scan_count;
	int scans[FISH_COUNT];
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

static void parse_round_input(void) {
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

	/* NOTE(benjamin): Drones are the first IDs so we can iterate on all fish by starting past them. */
	for (int i = TOTAL_DRONE_COUNT; i < ARRLEN(state.entities); i++) {
		state.entities[i].fish.visible = false;
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

static int target_type = 0;

static void play_drone(struct drone *drone, bool inverse_priority) {
	bool light = (drone->battery == DRONE_BATTERY_MAX);

	struct fish *target = NULL;
	int target_distance = 0;

#if 0
	for (int i = TOTAL_DRONE_COUNT; i < ARRLEN(state.entities); i++) {
		struct fish *fish = &state.entities[i].fish;
		if (!fish->visible || is_scanned(i)) { continue; }

		int fish_distance = abs_dist(drone->x, drone->y, fish->x, fish->y);

		if (!target) {
			target = fish;
			target_distance = fish_distance;
			continue;
		}

		if (fish_distance < target_distance) {
			target = fish;
			target_distance = fish_distance;
		}
	}
#endif

	if (target) {
		dbg("drone %d moving to visible target %d\n", ENTITY_ID(drone), ENTITY_ID(target));
		printf("MOVE %d %d %d\n", target->x, target->y, light);
		return;
	}

	if (7 <= drone->scan_count) {
		dbg("drone %d delivering scans\n", ENTITY_ID(drone));
		printf("MOVE %d 0 %d\n", drone->x, light);
		return;
	}

	enum direction target_direction = NO_DIRECTION;

	for (int i = 0; i < drone->blip_count; i++) {
		int creature_id = drone->blips[i].creature_id;
		if (is_scanned(creature_id)) { continue; }

		struct fish *fish = &state.entities[creature_id].fish;
		enum direction fish_direction = drone->blips[i].direction;

		if (!target ||
				(fish->type < target->type || inverse_priority) ||
				(target_direction < fish_direction || inverse_priority) ) {
			target = fish;
			target_direction = fish_direction;
			continue;
		}
	}

	if (target_direction == NO_DIRECTION) {
		dbg("drone %d finished\n", ENTITY_ID(drone));
		printf("MOVE %d 0 %d\n", drone->x, light);
		return;
	}

	int dest_x = drone->x;
	int dest_y = drone->y;
	switch (target_direction) {
		case BL: dest_x -= 300; dest_y += 300; break;
		case TL: dest_x -= 300; dest_y -= 300; break;
		case BR: dest_x += 300; dest_y += 300; break;
		case TR: dest_x += 300; dest_y -= 300; break;
		default: assert(1, "Invalid direction\n");
	}

	if (dest_x < 0) dest_x = 0;
	if (dest_x > 10000) dest_x = 10000;
	if (dest_y < 0) dest_y = 0;
	if (dest_y > 10000) dest_y = 10000;

	dbg("drone %d following radar to entity %d\n", ENTITY_ID(drone), ENTITY_ID(target));
	printf("MOVE %d %d %d\n", dest_x, dest_y, light);
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

		dbg("playing drone 1...\n");
		play_drone(&state.entities[state.my.drones[0]].drone, false);
		dbg("playing drone 2...\n");
		play_drone(&state.entities[state.my.drones[1]].drone, true);
	}

	return 0;
}
