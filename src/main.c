// Gang Mod: more friendly and enemy tanks in the campaign.
//
// In each campaign mission this adds the number of ally and enemy tanks
// picked in the Mods menu, of the tank class picked for each side:
// - Allies join your gang on its spawn spots a moment after the mission
//   starts. They hunt the enemy or escort you (Ally Orders).
// - Enemies join the first enemy gang that shows up, at that tank's
//   generator (where its gang brings in new tanks), or next to it.
// Both are the game's own AI tanks, made the way the game makes them, with
// their sight and rate of fire set by Ally Skill / Enemy Skill.
// Remove Trains is in trains.c. NOTES.md has the game details behind it all.

#include "modding.h"
#include "recompconfig.h"
#include "btga.h"
#include "trains.h"

// Slots left free for the game's own tanks. Extra enemies leave a few, so
// they don't crowd the mission's own. Allies leave one: your own respawn
// takes an AI tank's slot when none is free (func_8008AE04), so they can fill
// up. Some missions (Champs Elysee) start with 23 of the 24 slots in use;
// there allies wait for a slot instead (see step_side).
#define FREE_SLOTS_FOR_ENEMIES 4
#define FREE_SLOTS_FOR_ALLIES  1

// Wait this long into a mission before adding tanks (1/30 s), and this long
// between two tanks of a side, so they don't pile up.
#define START_DELAY   60
#define SPAWN_SPACING 10
// Give up on a side after this many tries without a free spot for a tank.
// (Waiting for a free slot doesn't count.)
#define MAX_TRIES     120

// Class options (mod.toml): 0 Random, 1 Same as the mission's, then the tanks.
#define CLASS_RANDOM  0
#define CLASS_MISSION 1
#define CLASS_FIRST   2
// Random picks from the ordinary tanks (no Attack Boat, no bosses).
#define RANDOM_TYPES  (TANK_HORNET + 1)
#define RANDOM_MASK   ((1 << RANDOM_TYPES) - 1)

typedef struct {
    u8 done;        // all added (or gave up)
    u8 made;        // tanks added so far
    u8 tries;       // spawn attempts without a free spot
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

static u8* make_tank(f32 x, f32 z, u16 heading, u8 layer, u8 gang, s32 type, s32 ai_state) {
    u8* g = BTGA_GANG(gang);
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
    return (u8*)func_8008AE04(&spawn);
}

// Keep this far from train tracks (a car is about 100 wide, a tank about 100
// long), so a train never runs into a new tank. Truck Stop: the road is about
// 230 from the nearer track, the gang base spots 170-180.
#define TRACK_CLEARANCE 150.0f

static s32 spot_free(s32 type, f32 x, f32 z, u16 heading, u8 layer) {
    if (near_train_track(x, z, TRACK_CLEARANCE)) {
        return 0;
    }
    return func_8008F4AC(type, (s32)x, (s32)z, heading, layer, 1, 0, 0) != 0;
}

// Makes one AI tank of `gang` at one of the gang's base spots (where the
// game brings in that gang's own tanks, so they can drive out from there).
// Returns it, or NULL if none is free.
static u8* spawn_at_base(u8 gang, s32 type, s32 ai_state) {
    u8* spots = GANG_BASE_SPOTS(BTGA_GANG(gang));
    if ((u32)spots < 0x80000000u || (u32)spots >= 0x80800000u) {
        return NULL;
    }
    // Spot 0 is where your own tank comes in: try it last.
    for (s32 k = 1; k <= GANG_BASE_SPOT_COUNT; k++) {
        u8* sp = spots + BASE_SPOT_SIZE * (k % GANG_BASE_SPOT_COUNT);
        if (spot_free(type, BASE_SPOT_X(sp), BASE_SPOT_Z(sp), BASE_SPOT_HEADING(sp), BASE_SPOT_LAYER(sp))) {
            return make_tank(BASE_SPOT_X(sp), BASE_SPOT_Z(sp), BASE_SPOT_HEADING(sp), BASE_SPOT_LAYER(sp),
                             gang, type, ai_state);
        }
    }
    return NULL;
}

// Makes one AI tank of `gang` near (cx, cz). Returns it, or NULL.
static u8* spawn_near(f32 cx, f32 cz, u16 heading, u8 layer, u8 gang, s32 type, s32 ai_state) {
    // Start the search in a random direction, so the tanks don't all line
    // up on the same side.
    s32 start = (s32)(rng_next() & 7);

    for (u32 d = 0; d < sizeof(ring_dist) / sizeof(ring_dist[0]); d++) {
        for (s32 k = 0; k < 8; k++) {
            s32 dir = (start + k) & 7;
            f32 x = cx + ring_dir[dir][0] * ring_dist[d];
            f32 z = cz + ring_dir[dir][1] * ring_dist[d];
            if (spot_free(type, x, z, heading, layer)) {
                return make_tank(x, z, heading, layer, gang, type, ai_state);
            }
        }
    }
    return NULL;
}

static void reset_side(Side* s) {
    s->done = 0;
    s->made = 0;
    s->tries = 0;
    s->next_time = 0;
}

// --- Escort: allies that stay with you and fight what comes near. ---
// They run the game's own hunt state (6), like the mission's allies, so they
// drive and shoot the same way. That state picks its target every frame
// (func_80088854), normally the first tank of the first enemy gang, wherever
// it is. Right after it does, the mod picks instead: the nearest enemy close
// to you or to the escort, or, with none near, you (to follow, not attack).

#define MAX_ESCORTS   16
#define ENGAGE_DIST   1600.0f   // enemies this close to you or an escort get attacked
#define LEASH_DIST    2600.0f   // never chase a target this far from you
#define FOLLOW_RANGE  400.0f    // how close they keep to you (the game's own follow value)

static u8* escorts[MAX_ESCORTS];
static s32 escort_count = 0;
static u8* escort_player = NULL;  // your tank this frame, or NULL
static u8* picking_for = NULL;    // the escort func_80088854 is running for

static f32 dist2(u8* a, u8* b) {
    f32 dx = TANK_X(a) - TANK_X(b);
    f32 dz = TANK_Z(a) - TANK_Z(b);
    return dx * dx + dz * dz;
}

static s32 is_enemy_of(u8* t, u8* me) {
    if (t == me || !tank_alive(t) || (TANK_FLAGS(t) & 0x80)) {
        return 0;
    }
    if (TANK_CONTROL(t) > 3 || TANK_LAYER(t) != TANK_LAYER(me)) {
        return 0;
    }
    u8* g = TANK_GANG_PTR(t);
    u8* mg = TANK_GANG_PTR(me);
    if (g == NULL || mg == NULL) {
        return 0;
    }
    return GANG_TEAM(g) != GANG_TEAM(mg);
}

// The nearest enemy of `me` within `max_dist` of `from`.
static u8* nearest_enemy(u8* me, u8* from, f32 max_dist) {
    u8* best = NULL;
    f32 best_d = max_dist * max_dist;
    for (s32 i = 0; i < BTGA_TANK_SLOTS; i++) {
        u8* t = tank_slot(i);
        if (!is_enemy_of(t, me)) {
            continue;
        }
        f32 d = dist2(t, from);
        if (d < best_d) {
            best_d = d;
            best = t;
        }
    }
    return best;
}

static s32 escort_index(u8* t) {
    for (s32 i = 0; i < escort_count; i++) {
        if (escorts[i] == t) {
            return i;
        }
    }
    return -1;
}

// Drops escorts that are gone (or whose slot now holds another tank).
static void escorts_tidy(void) {
    for (s32 i = 0; i < escort_count; i++) {
        u8* e = escorts[i];
        s32 control = TANK_CONTROL(e);
        if (!tank_alive(e) || (control != 1 && control != 2)) {
            escorts[i] = escorts[--escort_count];
            i--;
        }
    }
}

// --- Catch-up: the game's AI can't keep up with you at full speed, and can
// get stuck on the scenery. An escort more than CATCHUP_DIST away (far into
// the fog) is moved to a free spot behind you, the way you're driving.
#define CATCHUP_DIST  3000.0f
#define CATCHUP_EVERY 30         // check once a second (1/30 s)

static s32 catchup_next_time = 0;
static f32 player_last_x = 0.0f;
static f32 player_last_z = 0.0f;

static f32 approx_len(f32 x, f32 z) {
    // Good enough for a direction: max + half min.
    f32 ax = x < 0 ? -x : x;
    f32 az = z < 0 ? -z : z;
    return ax > az ? ax + az * 0.5f : az + ax * 0.5f;
}

static void escort_move_to(u8* e, u8* player, f32 x, f32 z) {
    TANK_X(e) = x;
    TANK_Z(e) = z;
    TANK_Y(e) = TANK_Y(player);
    TANK_PREV_X(e) = x;
    TANK_PREV_Z(e) = z;
    TANK_PREV_Y(e) = TANK_Y(player);
    TANK_NAV_X(e) = x;
    TANK_NAV_Z(e) = z;
    TANK_PLAN_X(e) = x;
    TANK_PLAN_Z(e) = z;
    TANK_HEADING(e) = TANK_HEADING(player);
    func_80088ABC(e, player, FOLLOW_RANGE, 0);
}

static void escorts_catch_up(s32 now, u8* player) {
    if (now < catchup_next_time) {
        return;
    }
    catchup_next_time = now + CATCHUP_EVERY;

    f32 px = TANK_X(player);
    f32 pz = TANK_Z(player);
    // Which way you're going: from where you were a second ago.
    f32 dx = px - player_last_x;
    f32 dz = pz - player_last_z;
    player_last_x = px;
    player_last_z = pz;
    f32 len = approx_len(dx, dz);
    if (len < 20.0f) {
        dx = 0.0f;
        dz = 0.0f;
    } else {
        dx /= len;
        dz /= len;
    }

    for (s32 i = 0; i < escort_count; i++) {
        u8* e = escorts[i];
        if (!tank_alive(e) || dist2(e, player) < CATCHUP_DIST * CATCHUP_DIST) {
            continue;
        }
        s32 type = TANK_TYPE(e);
        u8 layer = TANK_LAYER(player);
        u16 heading = TANK_HEADING(player);
        s32 done = 0;
        // Behind you first (then to the sides), a little farther each time.
        static const f32 back[] = { 500.0f, 700.0f, 900.0f };
        static const f32 side[] = { 0.0f, 300.0f, -300.0f };
        for (s32 b = 0; b < 3 && !done; b++) {
            for (s32 k = 0; k < 3 && !done; k++) {
                f32 x = px - dx * back[b] + dz * side[k];
                f32 z = pz - dz * back[b] - dx * side[k];
                if (dx == 0.0f && dz == 0.0f) {
                    x = px + ring_dir[(i + k) & 7][0] * back[b];
                    z = pz + ring_dir[(i + k) & 7][1] * back[b];
                }
                if (spot_free(type, x, z, heading, layer)) {
                    escort_move_to(e, player, x, z);
                    done = 1;
                }
            }
        }
    }
}

RECOMP_HOOK("func_80088854")
void gang_mod_pick_target_start(u8* tank) {
    picking_for = (escort_count > 0 && escort_index(tank) >= 0) ? tank : NULL;
}

RECOMP_HOOK_RETURN("func_80088854")
void gang_mod_pick_target_end(void) {
    u8* e = picking_for;
    u8* player = escort_player;
    picking_for = NULL;
    if (e == NULL || player == NULL || !tank_alive(e) || TANK_AI_STATE(e) != AI_HUNT ||
        TANK_GANG(e) != TANK_GANG(player)) {
        return;
    }

    // Attack what's near you or the escort: within ENGAGE_DIST, or as far as
    // the escort can see if that's farther.
    f32 engage = TANK_AI_SIGHT(e);
    if (!(engage > ENGAGE_DIST)) {
        engage = ENGAGE_DIST;
    }
    if (engage > LEASH_DIST) {
        engage = LEASH_DIST;
    }
    u8* target = nearest_enemy(e, player, engage);
    if (target == NULL) {
        target = nearest_enemy(e, e, engage);
        if (target != NULL && dist2(target, player) > LEASH_DIST * LEASH_DIST) {
            target = NULL;
        }
    }

    if (target != NULL) {
        f32 range = TANK_AI_RANGE(e);
        if (!(range > 50.0f && range < 5000.0f)) {
            range = 700.0f;
        }
        func_80088ABC(e, target, range, 1);
    } else if (!(TANK_AI_HAS_TARGET(e) && TANK_AI_TARGET(e) == player && !TANK_AI_ATTACK(e))) {
        func_80088ABC(e, player, FOLLOW_RANGE, 0);
    }
}

// Ally Skill / Enemy Skill (mod.toml): 0 Game (as made), 1 Veteran, 2 Elite.
// The game's skill levels (0x80125C30) run from shot delay 2.5 down to 1.0
// and sight 0.1 up to 0.99 (x the class's reach); the player's gang is at
// 2.5 and 0.4. Elite is the table's best level (8), Veteran in between.
typedef struct {
    f32 shot_delay;
    f32 sight;
    f32 range;
} Skill;

static const Skill skills[] = {
    { 0.0f, 0.0f, 0.0f },   // Game: leave as made
    { 1.6f, 0.75f, 0.3f },  // Veteran
    { 1.0f, 0.99f, 0.4f },  // Elite
};

static void apply_skill(u8* tank, u32 level) {
    if (level == 0 || level >= sizeof(skills) / sizeof(skills[0])) {
        return;
    }
    s32 type = TANK_TYPE(tank);
    u8* g = TANK_GANG_PTR(tank);
    if (type < 0 || type >= TANK_TYPE_COUNT || g == NULL) {
        return;
    }
    f32 reach = (f32)TANK_TYPE_AI_REACH(type);
    f32 divisor = GANG_SKILL_DIVISOR(g);
    if (!(divisor > 0.1f && divisor < 10.0f)) {
        divisor = 1.0f;
    }
    TANK_AI_SHOT_DELAY(tank) = skills[level].shot_delay / divisor;
    TANK_AI_SIGHT(tank) = skills[level].sight * reach;
    TANK_AI_RANGE(tank) = skills[level].range * reach;
}

// Ally Orders (mod.toml): 0 Auto, 1 Escort You, 2 Hunt Enemies.
#define ORDERS_AUTO   0
#define ORDERS_ESCORT 1
#define ORDERS_HUNT   2
// Enemy Orders: 0 Same as Theirs, 1 Hunt.
#define ENEMY_ORDERS_HUNT 1

// Auto: hunt in the levels whose goal is to destroy every enemy tank, escort
// in the rest (get somewhere, rescue, protect, destroy a building). Level ids
// and goals from func_800E8C88 / func_800E8E34.
static s32 level_hunts(s32 level) {
    switch (level) {
        case 26: // SF Airport: destroy all invading tanks
        case 1:  // Truck Stop: destroy all tanks and bunkers
        case 5:  // White House: destroy all enemies
        case 8:  // Tower of London: destroy all tanks and boats
        case 10: // Champs Elysee: destroy all tanks
        case 15: // Assault on SF: destroy all tanks and boats
        case 24: // Shore Patrol: destroy all tanks and boats
            return 1;
        default:
            return 0;
    }
}

static s32 allies_escort(u32 orders) {
    if (orders == ORDERS_AUTO) {
        return !level_hunts(BTGA_LEVEL_ID);
    }
    return orders == ORDERS_ESCORT;
}

// --- Room for allies in a full mission. Some missions fill almost every
// tank slot with their own enemies (Champs Elysee: 23 of 24), and they don't
// free up often. Then the mod does what the game does for your own respawn:
// it takes out one enemy AI tank, far out of sight, and gives it back to its
// tank generator to bring in again later, so the mission's army is the same.
#define ROOM_MIN_DIST  3500.0f   // only enemies at least this far from you
#define ROOM_EVERY     60        // at most one every 2 s (1/30 s)

static s32 room_next_time = 0;

static s32 make_room(s32 now, u8* player) {
    if (now < room_next_time) {
        return 0;
    }
    room_next_time = now + ROOM_EVERY;

    u8* best = NULL;
    f32 best_d = ROOM_MIN_DIST * ROOM_MIN_DIST;
    for (s32 i = 0; i < BTGA_TANK_SLOTS; i++) {
        u8* t = tank_slot(i);
        s32 control = TANK_CONTROL(t);
        if (!is_enemy_of(t, player) || (control != 1 && control != 2)) {
            continue;
        }
        if (TANK_TYPE(t) >= TANK_ATTACK_BOAT) {
            continue; // never boats or bosses
        }
        f32 d = dist2(t, player);
        if (d > best_d) {
            best_d = d;
            best = t;
        }
    }
    if (best == NULL) {
        return 0;
    }
    u8* spawner = TANK_SPAWNER(best);
    if (spawner != NULL && OBJ_TYPE(spawner) == OBJ_TYPE_TANK_SPAWNER && SPAWNER_LEFT(spawner) != 0xFFFF) {
        SPAWNER_LEFT(spawner)++;
    }
    func_8008B82C(best, 0x7F, 0);
    return 1;
}

// One step of a side: makes at most one tank. `wanted` is how many to add.
// `ai_state` < 0: take it from the reference tank's generator (enemies),
// or hunt if it has none.
static void step_side(Side* s, u32 wanted, s32 now, u8* ref, u32 class_option, s32 use_spawner, s32 ai_state, s32 escorting) {
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

    // No slot: wait. Allies check every frame, so when a tank dies they get
    // its slot before the mission's tank generators bring in another.
    s32 keep_free = (s == &allies) ? FREE_SLOTS_FOR_ALLIES : FREE_SLOTS_FOR_ENEMIES;
    if (tanks_in_use() >= BTGA_TANK_SLOTS - keep_free) {
        if (s != &allies || !escort_player || !make_room(now, escort_player) ||
            tanks_in_use() >= BTGA_TANK_SLOTS - keep_free) {
            s->next_time = (s == &allies) ? now : now + SPAWN_SPACING * 3;
            return;
        }
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
    u8* tank = NULL;
    if (s == &allies) {
        tank = spawn_at_base(TANK_GANG(ref), type, ai_state);
    }
    if (tank == NULL) {
        tank = spawn_near(cx, cz, heading, layer, TANK_GANG(ref), type, ai_state);
    }
    if (tank != NULL) {
        s->made++;
        s->tries = 0;
        apply_skill(tank, recomp_get_config_u32(s == &allies ? "ally_skill" : "enemy_skill"));
        if (s == &allies && escorting && escort_count < MAX_ESCORTS) {
            escorts[escort_count++] = tank;
        }
    } else {
        s->tries++;
    }
    s->next_time = now + SPAWN_SPACING;
}

// func_8007A0A0 starts each frame of the game's main loop.
RECOMP_HOOK("func_8007A0A0")
void gang_mod_on_frame(void) {
    trains_frame();

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
        escort_count = 0;
        catchup_next_time = 0;
        room_next_time = 0;
    }
    last_time = now;

    if (BTGA_GAME_MODE != BTGA_MODE_CAMPAIGN || BTGA_TANK_COUNT == 0 || now < START_DELAY) {
        return;
    }

    u8* player = find_player();
    escort_player = player;
    escorts_tidy();
    if (player == NULL) {
        return;
    }
    escorts_catch_up(now, player);
    rng_state ^= (u32)now;

    if (ally_count > 0) {
        // Both orders use the hunt state; Escort picks its targets (above).
        step_side(&allies, ally_count, now, player, ally_class, 0, AI_HUNT, allies_escort(ally_orders));
    }
    if (enemy_count > 0) {
        step_side(&enemies, enemy_count, now, find_enemy(TANK_GANG(player)), enemy_class, 1,
                  enemy_orders == ENEMY_ORDERS_HUNT ? AI_HUNT : -1, 0);
    }
}
