#ifndef BTGA_H
#define BTGA_H

// Basic N64 types.
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed long s32;
typedef unsigned long u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;

#define NULL ((void*)0)

// ---------------------------------------------------------------------------
// Tanks, gangs and missions. Found for this mod by reading the game's code
// (USA 1.0); see NOTES.md for where each one comes from.

// Game mode: 1 Campaign, 2 Deathmatch, 3 Battlelord, 4 Frenzy, 5 Hold 'Em,
// 6 Family Mode, 7 Tank Wars, 8 Convoy.
#define BTGA_GAME_MODE (*(volatile u32*)0x80117EB4)
#define BTGA_MODE_CAMPAIGN 1

// The level being played (the id func_800E8C88 / func_800E8E34 take for its
// name and goal text). See level_hunts() in src/main.c for the campaign ones.
#define BTGA_LEVEL_ID (*(volatile s32*)0x8021949C)

// Level clock, in 1/30 s. Set to 0 when a level starts.
#define BTGA_LEVEL_TIME (*(volatile s32*)0x8021945C)

// Tank objects: 24 slots of 0x4CC bytes, and how many have been made.
#define BTGA_TANK_SLOTS     24
#define BTGA_TANK_SIZE      0x4CC
#define BTGA_TANKS          ((u8*)0x801AD128)
#define BTGA_TANK_COUNT     (*(volatile u16*)0x801AD120)

// Fields of a tank object.
#define TANK_X(t)        (*(f32*)((t) + 0x08))
#define TANK_Z(t)        (*(f32*)((t) + 0x0C))
#define TANK_Y(t)        (*(f32*)((t) + 0x10))
#define TANK_PREV_X(t)   (*(f32*)((t) + 0x14)) // last frame's position
#define TANK_PREV_Z(t)   (*(f32*)((t) + 0x18))
#define TANK_PREV_Y(t)   (*(f32*)((t) + 0x1C))
#define TANK_PLAN_X(t)   (*(f32*)((t) + 0x110)) // where the AI planned its route from
#define TANK_PLAN_Z(t)   (*(f32*)((t) + 0x114))
#define TANK_NAV_X(t)    (*(f32*)((t) + 0x150)) // the position other AI head for
#define TANK_NAV_Z(t)    (*(f32*)((t) + 0x154))
#define TANK_HEADING(t)  (*(u16*)((t) + 0x20))
#define TANK_LAYER(t)    (*(u8*)((t) + 0x94))
#define TANK_GANG(t)     (*(u8*)((t) + 0x95))   // 0x7F = no gang
#define TANK_TYPE(t)     (*(s32*)((t) + 0x98))
#define TANK_CONTROL(t)  (*(s32*)((t) + 0x9C))  // 0 human, 1/2 AI
#define TANK_HEALTH(t)   (*(s32*)((t) + 0x1D4))
#define TANK_FLAGS(t)    (*(u32*)((t) + 0x1E0)) // bit 0: slot in use
#define TANK_SPAWNER(t)  (*(u8**)((t) + 0x4A0)) // the tank generator that made it, or NULL
#define TANK_GANG_PTR(t) (*(u8**)((t) + 0x1D0))
#define TANK_AI_STATE(t) (*(s32*)((t) + 0x168))
// AI skill, copied from the skill table (0x80125C30, 24 bytes per level,
// the gang's level at gang +0x1F0) by func_80082A90 when the tank is made:
#define TANK_AI_SHOT_DELAY(t) (*(f32*)((t) + 0x1B8)) // x the wait between volleys: (75..90 frames) * this * 0.25
#define TANK_AI_SIGHT(t)  (*(f32*)((t) + 0x1BC)) // how far it sees other tanks
#define TANK_AI_RANGE(t)  (*(f32*)((t) + 0x1C0)) // how close it gets to attack
#define TANK_AI_ATTACK(t) (*(u8*)((t) + 0xC4))  // 1: the target is to be attacked
#define TANK_AI_HAS_TARGET(t) (*(u8*)((t) + 0xC5))
#define TANK_AI_TARGET(t) (*(u8**)((t) + 0xC8))

// Gangs: 0x250 bytes each.
#define BTGA_GANG(i)        ((u8*)(0x80235F00 + 0x250 * (u32)(i)))
#define GANG_FLAGS(g)       (*(u8*)((g) + 0x0A))
#define GANG_SKILL_DIVISOR(g) (*(f32*)((g) + 0x1DC)) // divides the shot delay
#define GANG_TEAM(g)        (*(u32*)((g) + 0x10)) // gangs with the same team are allies
// The gang's base spots, where it brings in its own tanks (func_800A9080):
// 5 of 12 bytes, x and z (f32), heading (u16), layer (u8).
#define GANG_BASE_SPOTS(g)      (*(u8**)((g) + 0x1E8))
#define GANG_BASE_SPOT_COUNT    5
#define BASE_SPOT_SIZE          12
#define BASE_SPOT_X(s)          (*(f32*)((s) + 0x0))
#define BASE_SPOT_Z(s)          (*(f32*)((s) + 0x4))
#define BASE_SPOT_HEADING(s)    (*(u16*)((s) + 0x8))
#define BASE_SPOT_LAYER(s)      (*(u8*)((s) + 0xA))
#define GANG_WEAPON_A(g)    (*(s32*)((g) + 0x1F4))
#define GANG_WEAPON_B(g)    (*(s32*)((g) + 0x1F8))

// Generic game objects: a pool of 0x44-byte entries. +0x00 serial (0 = free),
// +0x04 type, +0x08 next index in its type's list (s16, -1 ends it), then the
// object's own data. Each type's list starts at BTGA_OBJ_HEAD(type); free
// entries are chained from BTGA_OBJ_FREE_HEAD. (func_800A18D0 makes one,
// func_800A1A28 walks a type, the loop at 0x800A1DDC runs and frees them.)
#define BTGA_OBJ_POOL        0x200
#define BTGA_OBJ(idx)        ((u8*)(0x80224EF0 + 0x44 * (s32)(idx)))
#define BTGA_OBJ_HEAD(type)  (*(volatile s16*)(0x80224E68 + 2 * (type)))
#define BTGA_OBJ_FREE_HEAD   (*(volatile s16*)0x80224B50)
#define OBJ_NEXT(o)          (*(volatile s16*)((o) + 0x08))
#define OBJ_TYPE_TRAIN       1
#define OBJ_TYPE_TRAIN_CAR   2
#define TRAIN_SOUND(o)       (*(volatile s32*)((o) + 0x40)) // its engine sound, 0 = none
// A train runs on a straight track: from (x, z) along the angle, for the
// length; its cars sit at distances along it (func_800EEADC).
#define TRAIN_TRACK_X(o)      (*(f32*)((o) + 0x0C))
#define TRAIN_TRACK_Z(o)      (*(f32*)((o) + 0x10))
#define TRAIN_TRACK_LENGTH(o) (*(f32*)((o) + 0x14))
#define TRAIN_TRACK_ANGLE(o)  (*(u16*)((o) + 0x24))

// sin and cos of a 16-bit angle (0x10000 = a full turn): the x and z of the
// direction the game uses for that angle.
f32 func_8009D4B0(s32 angle);
f32 func_8009D510(s32 angle);

// Stops a sound started with func_80097A6C.
void func_80097BA4(s32 handle, s32 a1);

// Tank generators etc. (first word after the link is the object's type).
#define OBJ_TYPE(o)          (*(s32*)((o) + 0x04))
#define OBJ_TYPE_TANK_SPAWNER 0x0D
#define SPAWNER_X(o)         (*(f32*)((o) + 0x0C))
#define SPAWNER_Z(o)         (*(f32*)((o) + 0x10))
#define SPAWNER_HEADING(o)   (*(u16*)((o) + 0x14))
#define SPAWNER_LAYER(o)     (*(u8*)((o) + 0x16))
#define SPAWNER_AI_STATE(o)  (*(u8*)((o) + 0x29)) // what its tanks do first
#define SPAWNER_LEFT(o)      (*(u16*)((o) + 0x18)) // tanks it still has to bring in (0xFFFF: no limit)

// Tank types (index into the table at 0x80122E30, 0xD0 bytes each).
enum {
    TANK_M1A1, TANK_MOTOTANK, TANK_GOLIATH, TANK_FLPE, TANK_HOVERTANK,
    TANK_RATTLER, TANK_RHINO, TANK_HYDRA, TANK_INFERNO, TANK_MARKSMAN,
    TANK_HORNET, TANK_ATTACK_BOAT, TANK_BOSS_A, TANK_BOSS_B, TANK_BOSS_C,
    TANK_TYPE_COUNT
};
#define TANK_TYPE_AI_REACH(i)   (*(u16*)(0x80122EE8 + 0xD0 * (u32)(i))) // sight and attack range = this * the skill's
#define TANK_TYPE_PART_COUNT(i) (*(s32*)(0x80122E30 + 0xD0 * (u32)(i)))
#define TANK_TYPE_PARTS(i)      (*(s32**)(0x80122E34 + 0xD0 * (u32)(i))) // pairs: part, model id

// Loaded models, by model id. NULL = not loaded in this level.
#define BTGA_MODELS ((void**)0x803A53A0)

// Campaign level records: 19 of 0x70 bytes. +0x4A is the level's
// tank-type mask (which tank models the level loads).
#define BTGA_LEVEL_COUNT 19
#define BTGA_LEVEL_TANK_MASK(i) (*(volatile u16*)(0x80124B90 + 0x70 * (u32)(i) + 0x4A))

// AI states (tank +0x168). A new AI tank drives clear of its spawn spot
// (state 25), then goes to the state it was made with.
#define AI_HOLD   4  // stop and stay put
#define AI_FOLLOW 5  // follow the gang's leader (a human player), fighting what it meets
#define AI_HUNT   6  // look for tanks of the other teams and attack them

// What func_8008AE04 takes to make a tank.
typedef struct {
    f32 x;          // 0x00
    f32 z;          // 0x04
    f32 y;          // 0x08 (0: on the ground)
    u16 heading;    // 0x0C
    u8 layer;       // 0x0E
    u8 gang;        // 0x0F
    s32 type;       // 0x10 tank type
    s32 control;    // 0x14 0 human, 1/2 AI
    s32 ai_state;   // 0x18 AI state once it has driven clear of its spawn spot (AI_*)
    u32 flags;      // 0x1C
    s32 weapon_a;   // 0x20
    s32 weapon_b;   // 0x24
    void* spawner;  // 0x28 the tank generator, or NULL
} TankSpawn;

// Makes a tank. Returns the tank object, or NULL when there's no free slot.
void* func_8008AE04(TankSpawn* spawn);

// Takes a tank out: its death, with `killer_gang` credited (0x7F: no one).
// `mode` 0 is what func_8008AE04 uses to make room for a human player's tank
// when every slot is taken. The slot is free again right after.
void func_8008B82C(void* tank, s32 killer_gang, s32 mode);

// Gives an AI tank a tank to go to: within `range`, and attacking it if
// `attack`. In state 5 (follow) the tank keeps to it while it's alive,
// then goes back to its gang's leader.
void func_80088ABC(void* tank, void* target, f32 range, s32 attack);

// Nonzero when a tank of this type fits at (x, z): on the map, on the
// ground, and clear of other tanks and objects.
s32 func_8008F4AC(s32 type, s32 x, s32 z, s32 heading, s32 layer, s32 check_ground, s32 alt, s32 ignore);

#endif
