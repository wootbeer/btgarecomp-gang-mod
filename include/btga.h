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

// Game variables we know about.
// From btgarecomp's src/game/cheats.cpp (the Cheats tab uses them).

// Tank Bucks. The GameShark code "Infinite Tank Bucks" (80236A9F 0064)
// keeps this at 100.
#define BTGA_TANK_BUCKS (*(volatile u8*)0x80236A9F)

// Flags the game's own cheat codes turn on (1 = on).
#define BTGA_CHEAT_INVINCIBILITY (*(volatile u8*)0x80125AB0) // HPPYHPPY
#define BTGA_CHEAT_ALL_WEAPONS   (*(volatile u8*)0x80125AB1) // RCKTSRDGLR

// ---------------------------------------------------------------------------
// Tanks, gangs and missions. Found for this mod by reading the game's code
// (USA 1.0); see NOTES.md for where each one comes from.

// Game mode: 1 Campaign, 2 Deathmatch, 3 Battlelord, 4 Frenzy, 5 Hold 'Em,
// 6 Family Mode, 7 Tank Wars, 8 Convoy.
#define BTGA_GAME_MODE (*(volatile u32*)0x80117EB4)
#define BTGA_MODE_CAMPAIGN 1

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
#define TANK_HEADING(t)  (*(u16*)((t) + 0x20))
#define TANK_LAYER(t)    (*(u8*)((t) + 0x94))
#define TANK_GANG(t)     (*(u8*)((t) + 0x95))   // 0x7F = no gang
#define TANK_TYPE(t)     (*(s32*)((t) + 0x98))
#define TANK_CONTROL(t)  (*(s32*)((t) + 0x9C))  // 0 human, 1/2 AI
#define TANK_HEALTH(t)   (*(s32*)((t) + 0x1D4))
#define TANK_FLAGS(t)    (*(u32*)((t) + 0x1E0)) // bit 0: slot in use
#define TANK_SPAWNER(t)  (*(u8**)((t) + 0x4A0)) // the tank generator that made it, or NULL

// Gangs: 0x250 bytes each.
#define BTGA_GANG(i)        ((u8*)(0x80235F00 + 0x250 * (u32)(i)))
#define GANG_FLAGS(g)       (*(u8*)((g) + 0x0A))
#define GANG_WEAPON_A(g)    (*(s32*)((g) + 0x1F4))
#define GANG_WEAPON_B(g)    (*(s32*)((g) + 0x1F8))

// Generic game objects (first word after the link is the object's type).
#define OBJ_TYPE(o)          (*(s32*)((o) + 0x04))
#define OBJ_TYPE_TANK_SPAWNER 0x0D
#define SPAWNER_X(o)         (*(f32*)((o) + 0x0C))
#define SPAWNER_Z(o)         (*(f32*)((o) + 0x10))
#define SPAWNER_HEADING(o)   (*(u16*)((o) + 0x14))
#define SPAWNER_LAYER(o)     (*(u8*)((o) + 0x16))

// Tank types (index into the table at 0x80122E30, 0xD0 bytes each).
enum {
    TANK_M1A1, TANK_MOTOTANK, TANK_GOLIATH, TANK_FLPE, TANK_HOVERTANK,
    TANK_RATTLER, TANK_RHINO, TANK_HYDRA, TANK_INFERNO, TANK_MARKSMAN,
    TANK_HORNET, TANK_ATTACK_BOAT, TANK_BOSS_A, TANK_BOSS_B, TANK_BOSS_C,
    TANK_TYPE_COUNT
};
#define TANK_TYPE_PART_COUNT(i) (*(s32*)(0x80122E30 + 0xD0 * (u32)(i)))
#define TANK_TYPE_PARTS(i)      (*(s32**)(0x80122E34 + 0xD0 * (u32)(i))) // pairs: part, model id

// Loaded models, by model id. NULL = not loaded in this level.
#define BTGA_MODELS ((void**)0x803A53A0)

// Campaign level records: 19 of 0x70 bytes. +0x4A is the level's
// tank-type mask (which tank models the level loads).
#define BTGA_LEVEL_COUNT 19
#define BTGA_LEVEL_TANK_MASK(i) (*(volatile u16*)(0x80124B90 + 0x70 * (u32)(i) + 0x4A))

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
    s32 ai_state;   // 0x18 first AI state; 4 = the normal one
    u32 flags;      // 0x1C
    s32 weapon_a;   // 0x20
    s32 weapon_b;   // 0x24
    void* spawner;  // 0x28 the tank generator, or NULL
} TankSpawn;

// Makes a tank. Returns the tank object, or NULL when there's no free slot.
void* func_8008AE04(TankSpawn* spawn);

// Nonzero when a tank of this type fits at (x, z): on the map, on the
// ground, and clear of other tanks and objects.
s32 func_8008F4AC(s32 type, s32 x, s32 z, s32 heading, s32 layer, s32 check_ground, s32 alt, s32 ignore);

#endif
