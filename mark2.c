/*
 * Wood League 2
 *
 * The blind habitat cleaner:
 * - single drone
 * - never use light
 * - scan all type 0 fish
 * - surface to register points
 * - repeat for types 1 and 2.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#define ARRLEN(x) (sizeof(x) / sizeof(*x))

#define MAX_X (10000)
#define MAX_Y (10000)

struct fish {
	int color;    /* [0,3] */
	int type;     /* [0,2] */
	int x;        /* [0, 10000] */
	int y;        /* [0, 10000] */
	int vx;       /* [0, 10000] ? */
	int vy;       /* [0, 10000] ? */
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
	/* NOTE(benjamin): arbitrary array size */
	struct radar_blip blips[32];
	int scan_count;
	/* NOTE(benjamin): arbitrary array size */
	int scans[32];
};

union entity {
	struct fish fish;
	struct drone drone;
};

struct player_state {
	int score;
	int scan_count;
	/* 3 types, 4 colors */
	int scans[12];
	int drone_id;
};

struct state {
	struct player_state my;
	struct player_state foe;
	/* NOTE(benjamin): arbitrary array size */
	union entity entities[32];
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
	assert(my_drone_count == 1, "Unexpected number of drones: %d\n", my_drone_count);

	scanf("%d", &state.my.drone_id);
	{
		struct drone *drone = &state.entities[state.my.drone_id].drone;
		scanf("%d%d%d%d", &drone->x, &drone->y, &drone->emergency, &drone->battery);
		drone->blip_count = 0;
		drone->scan_count = 0;
	}

	int foe_drone_count;
	scanf("%d", &foe_drone_count);
	assert(my_drone_count == 1, "Unexpected number of drones: %d\n", foe_drone_count);

	scanf("%d", &state.foe.drone_id);
	{
		struct drone *drone = &state.entities[state.foe.drone_id].drone;
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

	/* NOTE(benjamin): Drones will be ID 0 and 1, all subsequent IDs must be fish. */
	for (int i = 2; i < ARRLEN(state.entities); i++) {
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

		struct drone *my_drone = &state.entities[state.my.drone_id].drone;

		if (2 < type) {
			printf("MOVE %d 0 0\n", my_drone->x);
			continue;
		}

		if (my_drone->y < 500) {
			type += 1;
			printf("MOVE %d %d 0\n", my_drone->x, my_drone->y + 600);
			continue;
		}

		enum direction direction = TR;

		for (int i = 0; i < my_drone->blip_count; i++) {
			int creature_id = my_drone->blips[i].creature_id;

			struct fish *fish = &state.entities[creature_id].fish;
			if (fish->type != type) { continue; }

			for (int j = 0; j < state.my.scan_count; j++) {
				if (state.my.scans[j] == creature_id) { goto skip_creature; }
			}
			for (int j = 0; j < my_drone->scan_count; j++) {
				if (my_drone->scans[j] == creature_id) { goto skip_creature; }
			}

			enum direction blip_direction = my_drone->blips[i].direction;
			if (blip_direction < direction) {
				fprintf(stderr, "selecting %d in %s quadrant\n", creature_id,
					(blip_direction == BL) ? "BL" :
					(blip_direction == TL) ? "TL" :
					(blip_direction == BR) ? "BR" :
					(blip_direction == TR) ? "TR" : "XXX"
				);
				direction = blip_direction;
			}
skip_creature:
		}

		if (direction == NO_DIRECTION) {
			printf("MOVE %d 0 0\n", my_drone->x);
			continue;
		}

		int dest_x = my_drone->x;
		int dest_y = my_drone->y;
		fprintf(stderr, "@ %d,%d\n", dest_x, dest_y);
		switch (direction) {
			case BL:
				fprintf(stderr, "BL -300,+300\n");
				dest_x -= 300;
				dest_y += 300;
				break;
			case TL:
				fprintf(stderr, "TL -300,-300\n");
				dest_x -= 300;
				dest_y -= 300;
				break;
			case BR:
				fprintf(stderr, "BR +300,+300\n");
				dest_x += 300;
				dest_y += 300;
				break;
			case TR:
				fprintf(stderr, "TR +300,-300\n");
				dest_x += 300;
				dest_y -= 300;
				break;
			default:
				assert(1, "Invalid direction\n");
		}

		if (dest_x < 0) dest_x = 0;
		if (dest_x > 10000) dest_x = 10000;
		if (dest_y < 0) dest_y = 0;
		if (dest_y > 10000) dest_y = 10000;

		printf("MOVE %d %d 0\n", dest_x, dest_y);
	}

	return 0;
}
