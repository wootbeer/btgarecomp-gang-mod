#ifndef RECOMPUTILS_H
#define RECOMPUTILS_H

#include "modding.h"

// Get and free memory.
RECOMP_IMPORT("*", void* recomp_alloc(unsigned long size));
RECOMP_IMPORT("*", void recomp_free(void* memory));

// Note: recomp_printf isn't in btgarecomp yet, so it's not here. A mod
// that uses it won't load.

typedef enum {
    DEPENDENCY_STATUS_FOUND = 0,
    DEPENDENCY_STATUS_INVALID_DEPENDENCY = 1,
    DEPENDENCY_STATUS_NOT_FOUND = 2,
    DEPENDENCY_STATUS_WRONG_VERSION = 3
} DependencyStatus;

// Check if an optional dependency (from mod.toml) is there.
RECOMP_IMPORT("*", DependencyStatus recomp_is_dependency_met(const char* dependency_id));

#endif
