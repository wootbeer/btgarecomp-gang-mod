// Gang Mod: more friendly and enemy tanks in the campaign.
//
// In each campaign mission this adds the number of ally and enemy tanks
// picked in the Mods menu, of the tank class picked for each side:
// - Allies join your gang next to your tank a moment after the mission starts.
// - Enemies join the first enemy gang that shows up, at that tank's
//   generator (where its gang brings in new tanks), or next to it.
// Both are the game's own AI tanks, made the way the game makes them.

#include "modding.h"
#include "recompconfig.h"
#include "btga.h"

// Never fill the last few tank slots: the game needs them for its own
// tanks (generators, convoys, your respawn).
#define FREE_SLOTS_TO_KEEP 4

// Wait this long into a mission before adding tanks (1/30 s), and this long
// between two tanks of a side, so they don't pile up.
#define START_DELAY   60
#define SPAWN_SPACING 10
// Give up on a side after this many tries without room for a tank.
#define MAX_TRIES     120

// Class options (mod.toml): 0 Random, 1 Same as the mission's, then the tanks.
#define CLASS_RANDOM  0
#define CLASS_MISSION 1
#define CLASS_FIRST   2
// Random picks from the ordinary tanks (no Attack Boat, no bosses).
#define RANDOM_TYPES  (TANK_HORNET + 1)
#define RANDOM_MASK   ((1 << RANDOM_TYPES) - 1)

typedef struct {
    u8 started;     // this mission's spawning has begun
    u8 done;        // all added (or gave up)
    u8 made;        // tanks added so far
    u8 tries;       // spawn attempts without room
    s32 next_time;  // level time for the next try
} Side;

static Side allies;
static Side enemies;
static s32 last_time = -1;
static u32 rng_state = 0x1234567;

// Spots to try around a point: 8 directions at a few distances.
static const f32 ring_dir[8][2] = {
    { 1.0f, 0.0f }, { 0.7071f, 0.7071f }, { 0.0f, 1.0f }, { -0.7071f, 0.7071f },
    { -1.0f, 0.0f }, { -0.7071f, -0.7071f }, { 0.0f, -1.0f }, { 0.7071f, -0.7071f },
};
static const f32 ring_dist[] = { 220.0f, 320.0f, 440.0f, 580.0f, 750.0f };

static u32 rng_next(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state >> 16;
}

static u8* tank_slot(s32 i) {
    return BTGA_TANKS + BTGA_TANK_SIZE * i;
}

static s32 tank_alive(u8* t) {
    return (TANK_FLAGS(t) & 1) && TANK_HEALTH(t) > 0;
}

static s32 tanks_in_use(void) {
    s32 n = 0;
    for (s32 i = 0; i < BTGA_TANK_SLOTS; i++) {
        if (TANK_FLAGS(tank_slot(i)) & 1) {
            n++;
        }
    }
    return n;
}

// The human player's tank (player 1 if there are more).
static u8* find_player(void) {
    for (s32 i = 0; i < BTGA_TANK_SLOTS; i++) {
        u8* t = tank_slot(i);
        if (tank_alive(t) && TANK_CONTROL(t) == 0 && TANK_GANG(t) != 0x7F) {
            return t;
        }
    }
    return NULL;
}

// An enemy AI tank: another gang, AI-driven. Prefer one that came from a
// tank generator, since that's where its gang brings in tanks.
static u8* find_enemy(u8 player_gang) {
    u8* any = NULL;
    for (s32 i = 0; i < BTGA_TANK_SLOTS; i++) {
        u8* t = tank_slot(i);
        s32 control = TANK_CONTROL(t);
        if (!tank_alive(t) || TANK_GANG(t) == 0x7F || TANK_GANG(t) == player_gang) {
            continue;
        }
        if (control != 1 && control != 2) {
            continue;
        }
        if (TANK_TYPE(t) >= TANK_ATTACK_BOAT) {
            continue; // boats and bosses aren't a good model for a spawn spot
        }
        u8* spawner = TANK_SPAWNER(t);
        if (spawner != NULL && OBJ_TYPE(spawner) == OBJ_TYPE_TANK_SPAWNER) {
            return t;
        }
        if (any == NULL) {
            any = t;
        }
    }
    return any;
}

// True when every model of this tank type is loaded in this level.
static s32 type_loaded(s32 type) {
    if (type < 0 || type >= TANK_TYPE_COUNT) {
        return 0;
    }
    s32 parts = TANK_TYPE_PART_COUNT(type);
    s32* list = TANK_TYPE_PARTS(type);
    if (parts <= 0 || parts > 32 || list == NULL) {
        return 0;
    }
    for (s32 i = 0; i < parts; i++) {
        s32 model = list[i * 2 + 1];
        if (model < 0 || model >= 0x400 || BTGA_MODELS[model] == NULL) {
            return 0;
        }
    }
    return 1;
}

// The tank type to make for a side, from its class option. Falls back to
// the reference tank's type if the one picked isn't loaded.
static s32 pick_type(u32 option, s32 fallback) {
    s32 type = fallback;
    if (option == CLASS_RANDOM) {
        // A few tries for one that's loaded.
        for (s32 i = 0; i < 8; i++) {
            s32 t = (s32)(rng_next() % RANDOM_TYPES);
            if (type_loaded(t)) {
                return t;
            }
        }
    } else if (option >= CLASS_FIRST) {
        type = (s32)(option - CLASS_FIRST);
    }
    if (!type_loaded(type)) {
        type = fallback;
    }
    return type;
}

// Tank-type bits a class option needs loaded.
static u16 class_mask(u32 option) {
    if (option == CLASS_RANDOM) {
        return RANDOM_MASK;
    }
    if (option >= CLASS_FIRST && option - CLASS_FIRST < RANDOM_TYPES) {
        return (u16)(1 << (option - CLASS_FIRST));
    }
    return 0;
}

// Makes one AI tank of `gang` near (cx, cz). Returns 1 if it made one.
static s32 spawn_near(f32 cx, f32 cz, u16 heading, u8 layer, u8 gang, s32 type, s32 ai_state) {
    u8* g = BTGA_GANG(gang);
    // Start the search in a random direction, so the tanks don't all line
    // up on the same side.
    s32 start = (s32)(rng_next() & 7);

    for (u32 d = 0; d < sizeof(ring_dist) / sizeof(ring_dist[0]); d++) {
        for (s32 k = 0; k < 8; k++) {
            s32 dir = (start + k) & 7;
            f32 x = cx + ring_dir[dir][0] * ring_dist[d];
            f32 z = cz + ring_dir[dir][1] * ring_dist[d];
            if (!func_8008F4AC(type, (s32)x, (s32)z, heading, layer, 1, 0, 0)) {
                continue;
            }

            TankSpawn spawn;
            spawn.x = x;
            spawn.z = z;
            spawn.y = 0.0f;
            spawn.heading = heading;
            spawn.layer = layer;
            spawn.gang = gang;
            spawn.type = type;
            // The game's tank generators use 2 for gangs with flag 2, else 1.
            spawn.control = (GANG_FLAGS(g) & 2) ? 2 : 1;
            spawn.ai_state = ai_state;
            spawn.flags = 0;
            spawn.weapon_a = GANG_WEAPON_A(g);
            spawn.weapon_b = GANG_WEAPON_B(g);
            spawn.spawner = NULL;
            return func_8008AE04(&spawn) != NULL;
        }
    }
    return 0;
}

static void reset_side(Side* s) {
    s->started = 0;
    s->done = 0;
    s->made = 0;
    s->tries = 0;
    s->next_time = 0;
}

// One step of a side: makes at most one tank. `wanted` is how many to add.
// `ai_state` < 0: take it from the reference tank's generator (enemies),
// or hunt if it has none.
static void step_side(Side* s, u32 wanted, s32 now, u8* ref, u32 class_option, s32 use_spawner, s32 ai_state) {
    if (s->done) {
        return;
    }
    if (s->made >= wanted || s->tries >= MAX_TRIES) {
        s->done = 1;
        return;
    }
    if (ref == NULL || now < s->next_time) {
        return;
    }
    s->started = 1;

    if (tanks_in_use() >= BTGA_TANK_SLOTS - FREE_SLOTS_TO_KEEP) {
        s->tries++;
        s->next_time = now + SPAWN_SPACING * 3;
        return;
    }

    f32 cx = TANK_X(ref);
    f32 cz = TANK_Z(ref);
    u16 heading = TANK_HEADING(ref);
    u8 layer = TANK_LAYER(ref);
    if (use_spawner) {
        u8* spawner = TANK_SPAWNER(ref);
        if (spawner != NULL && OBJ_TYPE(spawner) == OBJ_TYPE_TANK_SPAWNER) {
            cx = SPAWNER_X(spawner);
            cz = SPAWNER_Z(spawner);
            heading = SPAWNER_HEADING(spawner);
            layer = SPAWNER_LAYER(spawner);
            if (ai_state < 0) {
                ai_state = SPAWNER_AI_STATE(spawner);
                // Standing still (0, 4) or no state at all: hunt instead.
                if (ai_state == 0 || ai_state == AI_HOLD || ai_state >= 31) {
                    ai_state = AI_HUNT;
                }
            }
        }
    }
    if (ai_state < 0) {
        ai_state = AI_HUNT;
    }

    s32 type = pick_type(class_option, TANK_TYPE(ref));
    if (spawn_near(cx, cz, heading, layer, TANK_GANG(ref), type, ai_state)) {
        s->made++;
        s->tries = 0;
    } else {
        s->tries++;
    }
    s->next_time = now + SPAWN_SPACING;
}

// func_8007A0A0 starts each frame of the game's main loop.
RECOMP_HOOK("func_8007A0A0")
void gang_mod_on_frame(void) {
    u32 ally_count = recomp_get_config_u32("ally_count");
    u32 ally_class = recomp_get_config_u32("ally_class");
    u32 enemy_count = recomp_get_config_u32("enemy_count");
    u32 enemy_class = recomp_get_config_u32("enemy_class");
    u32 ally_orders = recomp_get_config_u32("ally_orders");
    u32 enemy_orders = recomp_get_config_u32("enemy_orders");

    // Make the levels load the models of the classes picked (the same
    // place the "All Tanks All Levels" code writes). Only adds bits.
    u16 mask = 0;
    if (ally_count > 0) {
        mask |= class_mask(ally_class);
    }
    if (enemy_count > 0) {
        mask |= class_mask(enemy_class);
    }
    if (mask != 0) {
        for (s32 i = 0; i < BTGA_LEVEL_COUNT; i++) {
            u16 cur = BTGA_LEVEL_TANK_MASK(i);
            if ((cur & mask) != mask) {
                BTGA_LEVEL_TANK_MASK(i) = cur | mask;
            }
        }
    }

    s32 now = BTGA_LEVEL_TIME;
    // A new level (or a restart): the level clock went back.
    if (now < last_time) {
        reset_side(&allies);
        reset_side(&enemies);
    }
    last_time = now;

    if (BTGA_GAME_MODE != BTGA_MODE_CAMPAIGN || BTGA_TANK_COUNT == 0 || now < START_DELAY) {
        return;
    }

    u8* player = find_player();
    if (player == NULL) {
        return;
    }
    rng_state ^= (u32)now;

    if (ally_count > 0) {
        step_side(&allies, ally_count, now, player, ally_class, 0,
                  ally_orders == 1 ? AI_HUNT : AI_FOLLOW);
    }
    if (enemy_count > 0) {
        step_side(&enemies, enemy_count, now, find_enemy(TANK_GANG(player)), enemy_class, 1,
                  enemy_orders == 1 ? AI_HUNT : -1);
    }
}
