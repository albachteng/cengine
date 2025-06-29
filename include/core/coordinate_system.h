#ifndef COORDINATE_SYSTEM_H
#define COORDINATE_SYSTEM_H

/**
 * Centralized coordinate system constants
 * 
 * This header defines the shared coordinate system used across physics and rendering.
 * All world-space to screen-space conversions MUST use these constants to prevent
 * scaling mismatches between systems.
 * 
 * IMPORTANT: Modify these values carefully - changes affect both physics boundary
 * and rendering scaling simultaneously.
 */

// World coordinate system bounds - optimized for visibility
#define WORLD_BOUNDARY_RADIUS 100.0f    // Physics simulation boundary radius 
#define WORLD_COORDINATE_SCALE 120.0f   // Scale factor for world-to-screen conversion (slightly larger than boundary for padding)

// Derived rendering constants (DO NOT MODIFY - computed from world bounds)
#define RENDER_COORD_SCALE_X WORLD_COORDINATE_SCALE
#define RENDER_COORD_SCALE_Y WORLD_COORDINATE_SCALE  
#define RENDER_SCALE_FACTOR WORLD_COORDINATE_SCALE

// Validation: Ensure physics and rendering use consistent scaling
// (Runtime validation is implemented in the coordinate system test)

// Utility functions for coordinate conversion
static inline float world_to_screen_x(float world_x) {
    return world_x / WORLD_COORDINATE_SCALE;
}

static inline float world_to_screen_y(float world_y) {
    return world_y / WORLD_COORDINATE_SCALE;
}

static inline float screen_to_world_x(float screen_x) {
    return screen_x * WORLD_COORDINATE_SCALE;
}

static inline float screen_to_world_y(float screen_y) {
    return screen_y * WORLD_COORDINATE_SCALE;
}

#endif // COORDINATE_SYSTEM_H