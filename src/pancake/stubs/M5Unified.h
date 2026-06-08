#pragma once
// M5Unified.h stub for PORKCHOP PANCAKE build
// This file shadows the real M5Unified.h library header.
// It is found first because src/pancake/stubs is at the front
// of the include path (-I src/pancake/stubs in build_flags).
//
// On the Pancake build we don't have M5Unified installed.
// Instead we pull in pancake_hal_impl.h which provides all the
// compatible shim types (M5Canvas, M5GFX, Speaker_Class, etc.)

#ifdef PORKCHOP_PANCAKE
  #include "../pancake_hal_impl.h"
#endif
