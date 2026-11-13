#ifndef RECOMPCONFIG_H
#define RECOMPCONFIG_H

#include "modding.h"

// Read your mod's options (the [[manifest.config_options]] in mod.toml).
// Use u32 for Enum and Number options, string for String options.
RECOMP_IMPORT("*", unsigned long recomp_get_config_u32(const char* key));
RECOMP_IMPORT("*", double recomp_get_config_double(const char* key));
RECOMP_IMPORT("*", char* recomp_get_config_string(const char* key));

// Call this when you're done with a string from recomp_get_config_string.
RECOMP_IMPORT("*", void recomp_free_config_string(char* str));

// Your mod's version from mod.toml.
RECOMP_IMPORT("*", void recomp_get_mod_version(unsigned long* major, unsigned long* minor, unsigned long* patch));

#endif
