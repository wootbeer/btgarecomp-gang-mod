// Remove Trains: takes the trains (and their cars) out of every level.
//
// Trains are generic game objects of type 1, their cars type 2 (see
// NOTES.md). They're made by level code, so the mod removes them once
// they're there: it stops the train's engine sound, then unlinks each object
// from its type's list and gives it back to the free list, the same steps
// the game's object loop takes for an object that's done (0x800A1E94).

#include "modding.h"
#include "recompconfig.h"
#include "btga.h"
#include "trains.h"

// Unlinks object `idx` of `type` and frees it. Returns 1 if it was found.
static s32 free_object(s32 type, s16 idx) {
    volatile s16* link = &BTGA_OBJ_HEAD(type);
    s32 guard = 0;
    while (*link != -1 && guard++ < BTGA_OBJ_POOL) {
        if (*link == idx) {
            u8* o = BTGA_OBJ(idx);
            *(volatile u32*)o = 0;            // the slot is free
            *link = OBJ_NEXT(o);              // out of the type's list
            OBJ_NEXT(o) = BTGA_OBJ_FREE_HEAD; // onto the free list
            BTGA_OBJ_FREE_HEAD = idx;
            return 1;
        }
        link = &OBJ_NEXT(BTGA_OBJ(*link));
    }
    return 0;
}

static void free_all(s32 type) {
    s32 guard = 0;
    while (BTGA_OBJ_HEAD(type) != -1 && guard++ < BTGA_OBJ_POOL) {
        s16 idx = BTGA_OBJ_HEAD(type);
        if (type == OBJ_TYPE_TRAIN) {
            u8* o = BTGA_OBJ(idx);
            if (TRAIN_SOUND(o) != 0) {
                func_80097BA4(TRAIN_SOUND(o), 0);
                TRAIN_SOUND(o) = 0;
            }
        }
        if (!free_object(type, idx)) {
            break;
        }
    }
}

void trains_frame(void) {
    if (recomp_get_config_u32("remove_trains") == 0) {
        return;
    }
    // Only while a level is running (the clock is past its first second),
    // outside the object loop (this runs at the start of the frame).
    if (BTGA_LEVEL_TIME < 30) {
        return;
    }
    if (BTGA_OBJ_HEAD(OBJ_TYPE_TRAIN) == -1 && BTGA_OBJ_HEAD(OBJ_TYPE_TRAIN_CAR) == -1) {
        return;
    }
    free_all(OBJ_TYPE_TRAIN_CAR);
    free_all(OBJ_TYPE_TRAIN);
}

// True when (x, z) is within `clearance` of any train's track.
s32 near_train_track(f32 x, f32 z, f32 clearance) {
    s16 idx = BTGA_OBJ_HEAD(OBJ_TYPE_TRAIN);
    s32 guard = 0;
    while (idx != -1 && guard++ < BTGA_OBJ_POOL) {
        u8* t = BTGA_OBJ(idx);
        f32 x0 = TRAIN_TRACK_X(t);
        f32 z0 = TRAIN_TRACK_Z(t);
        f32 len = TRAIN_TRACK_LENGTH(t);
        f32 dx = func_8009D4B0(TRAIN_TRACK_ANGLE(t));
        f32 dz = func_8009D510(TRAIN_TRACK_ANGLE(t));
        // The nearest point of the track to (x, z).
        f32 along = (x - x0) * dx + (z - z0) * dz;
        if (along < -clearance) {
            along = -clearance;
        }
        if (len > 0.0f && along > len + clearance) {
            along = len + clearance;
        }
        f32 nx = x0 + dx * along - x;
        f32 nz = z0 + dz * along - z;
        if (nx * nx + nz * nz < clearance * clearance) {
            return 1;
        }
        idx = OBJ_NEXT(t);
    }
    return 0;
}
