/*
 * - single drone
 * - chase closest visible unscanned fish
 * - never light up
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#define ARRLEN(x) (sizeof(x) / sizeof(*x))

#define MAX_X (10000)
#define MAX_Y (10000)

#define MAX_CREATURE_COLOR (3)
#define MAX_CREATURE_TYPE (2)

/* NOTE(benjamin): arbitrary value */
#define MAX_CREATURES (32)

/* NOTE(benjamin): arbitrary value */
#define MAX_VISIBLE (32)

/* NOTE(benjamin): arbitrary value */
#define MAX_RADAR_BLIPS (128)

struct creature {
	int id;
	int color; /* [0,3] */
	int type;  /* [0,2] */
};

struct visible_creature {
	int id;
	int x;         /* [0, 10000] */
	int y;         /* [0, 10000] */
	int vx;         /* [0, 10000] ? */
	int vy;         /* [0, 10000] ? */
};

struct drone {
	int id;
	int x;         /* [0, 10000] */
	int y;         /* [0, 10000] */
	int emergency; /* ??? */
	int battery;   /* [0, 30] */
};

struct player_state {
	int score;
	int scan_count;
	int scans[MAX_CREATURES];
	struct drone drone;
};

struct radar_blip {
	int drone_id;
	int creature_id;
	char radar[3];
};

struct state {
	struct player_state my;
	struct player_state foe;
	int creature_count;
	struct creature creatures[MAX_CREATURES];
	int visible_creature_count;
	struct visible_creature visible_creatures[MAX_VISIBLE];
	int radar_blip_count;
	struct radar_blip radar_blips[MAX_RADAR_BLIPS];
};

static struct state state;

static void assert(bool cond, char *fmt, ...) {
	if (!cond) {
		va_list args;
		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
		abort();
	}
}

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

	int my_drone_count;
	scanf("%d", &my_drone_count);
	assert(my_drone_count == 1, "Unexpected number of drones: %d", my_drone_count);

	scanf("%d%d%d%d%d", &state.my.drone.id, &state.my.drone.x, &state.my.drone.y, &state.my.drone.emergency, &state.my.drone.battery);

	int foe_drone_count;
	scanf("%d", &foe_drone_count);
	assert(my_drone_count == 1, "Unexpected number of drones: %d", foe_drone_count);

	scanf("%d%d%d%d%d", &state.foe.drone.id, &state.foe.drone.x, &state.foe.drone.y, &state.foe.drone.emergency, &state.foe.drone.battery);

	int drone_scan_count;
	scanf("%d", &drone_scan_count);

	for (int i = 0; i < drone_scan_count; i++) {
		int drone_id;
		int creature_id;
		scanf("%d%d", &drone_id, &creature_id);
	}

	scanf("%d", &state.visible_creature_count);
	assert(
		0 <= state.visible_creature_count && state.visible_creature_count < ARRLEN(state.visible_creatures),
		"Unexpected visible creature count: %d", state.visible_creature_count
	);

	for (int i = 0; i < state.visible_creature_count; i++) {
		scanf(
			"%d%d%d%d%d", &state.visible_creatures[i].id, &state.visible_creatures[i].x,
			&state.visible_creatures[i].y, &state.visible_creatures[i].vx, &state.visible_creatures[i].vy
		);
	}

	scanf("%d", &state.radar_blip_count);
	assert(
		0 <= state.radar_blip_count && state.radar_blip_count < ARRLEN(state.radar_blips),
		"Unexpected radar blip count: %d", state.radar_blip_count
	);

	for (int i = 0; i < state.radar_blip_count; i++) {
		scanf("%d%d%s", &state.radar_blips[i].drone_id, &state.radar_blips[i].creature_id, state.radar_blips[i].radar);
	}
}

static void dump_state() {
#if 0
	fprintf(stderr, "state.my.score: %d\n", state.my.score);
	fprintf(stderr, "state.my.scan_count: %d\n", state.my.scan_count);
	for (int i = 0; i < state.my.scan_count; i++) {
		fprintf(stderr, "state.my.scans[%d]: %d\n", i, state.my.scans[i]);
	}
	fprintf(stderr, "state.my.drone.id: %d\n", state.my.drone.id);
	fprintf(stderr, "state.my.drone.x: %d\n", state.my.drone.x);
	fprintf(stderr, "state.my.drone.y: %d\n", state.my.drone.y);
	fprintf(stderr, "state.my.drone.emergency: %d\n", state.my.drone.emergency);
	fprintf(stderr, "state.my.drone.battery: %d\n", state.my.drone.battery);
	fprintf(stderr, "state.foe.score: %d\n", state.foe.score);
	fprintf(stderr, "state.foe.scan_count: %d\n", state.foe.scan_count);
	for (int i = 0; i < state.foe.scan_count; i++) {
		fprintf(stderr, "state.foe.scans[%d]: %d\n", i, state.foe.scans[i]);
	}
	fprintf(stderr, "state.foe.drone.id: %d\n", state.foe.drone.id);
	fprintf(stderr, "state.foe.drone.x: %d\n", state.foe.drone.x);
	fprintf(stderr, "state.foe.drone.y: %d\n", state.foe.drone.y);
	fprintf(stderr, "state.foe.drone.emergency: %d\n", state.foe.drone.emergency);
	fprintf(stderr, "state.foe.drone.battery: %d\n", state.foe.drone.battery);
	fprintf(stderr, "state.creature_count: %d\n", state.creature_count);
	for (int i = 0; i < state.creature_count; i++) {
		fprintf(stderr, "state.creatures[%d].id: %d\n", i, state.creatures[i].id);
		fprintf(stderr, "state.creatures[%d].color: %d\n", i, state.creatures[i].color);
		fprintf(stderr, "state.creatures[%d].type: %d\n", i, state.creatures[i].type);
	}
	fprintf(stderr, "state.visible_creature_count: %d\n", state.visible_creature_count);
	for (int i = 0; i < state.visible_creature_count; i++) {
		fprintf(stderr, "state.visible_creatures[%d].id: %d\n", i, state.visible_creatures[i].id);
		fprintf(stderr, "state.visible_creatures[%d].x: %d\n", i, state.visible_creatures[i].x);
		fprintf(stderr, "state.visible_creatures[%d].y: %d\n", i, state.visible_creatures[i].y);
		fprintf(stderr, "state.visible_creatures[%d].vx: %d\n", i, state.visible_creatures[i].vx);
		fprintf(stderr, "state.visible_creatures[%d].vy: %d\n", i, state.visible_creatures[i].vy);
	}
	fprintf(stderr, "state.radar_blip_count: %d\n", state.radar_blip_count);
	for (int i = 0; i < state.radar_blip_count; i++) {
		fprintf(stderr, "state.radar_blips[%d].drone_id: %d\n", i, state.radar_blips[i].drone_id);
		fprintf(stderr, "state.radar_blips[%d].creature_id: %d\n", i, state.radar_blips[i].creature_id);
		fprintf(stderr, "state.radar_blips[%d].radar[0]: %c\n", i, state.radar_blips[i].radar[0]);
		fprintf(stderr, "state.radar_blips[%d].radar[1]: %c\n", i, state.radar_blips[i].radar[1]);
		fprintf(stderr, "state.radar_blips[%d].radar[2]: %c\n", i, state.radar_blips[i].radar[2]);
	}
#endif
}

static int abs_dist(int ax, int ay, int bx, int by) {
	int dx = ax - bx;
	int dy = ay - by;

	return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

/**
 * Score points by scanning valuable fish faster than your opponent.
 **/

int main()
{
	scanf("%d", &state.creature_count);
	assert(0 <= state.creature_count && state.creature_count < ARRLEN(state.creatures), "Unexpected creature count: %d", state.creature_count);

	for (int i = 0; i < state.creature_count; i++) {
		scanf("%d%d%d", &state.creatures[i].id, &state.creatures[i].color, &state.creatures[i].type);
	}

	// game loop
	while (1) {
		parse_round_input();
		dump_state();

		if (!state.visible_creature_count) {
			continue;
		}

		struct visible_creature *closest_visible = NULL;
		int smallest_abs_dist;

		for (int i = 1; i < state.visible_creature_count; i++) {
			struct visible_creature *creature = &state.visible_creatures[i];

			for (int j = 0; j < state.my.scan_count; j++) {
				if (creature->id == state.my.scans[j]) {
					goto skip_creature;
				}
			}

			if (!closest_visible) {
				smallest_abs_dist = abs_dist(state.my.drone.x, state.my.drone.y, state.visible_creatures[i].x, state.visible_creatures[i].y);
				closest_visible = creature;
			}
			else {
				int cur_abs_dist = abs_dist(state.my.drone.x, state.my.drone.y, state.visible_creatures[i].x, state.visible_creatures[i].y);

				if (cur_abs_dist < smallest_abs_dist) {
					smallest_abs_dist = cur_abs_dist;
					closest_visible = &state.visible_creatures[i];
				}
			}

skip_creature:
		}

		fprintf(stderr, "closest unscanned is %d (abs_dist: %d)", closest_visible->id, smallest_abs_dist);

		printf("MOVE %d %d 0\n", closest_visible->x, closest_visible->y);
	}

	return 0;
}
