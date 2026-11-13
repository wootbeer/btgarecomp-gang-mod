#ifndef TRAINS_H
#define TRAINS_H
void trains_frame(void);

// True when (x, z) is within `clearance` of any train's track.
s32 near_train_track(f32 x, f32 z, f32 clearance);
#endif
