#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

/**
 * Centralized Display and Map Configuration
 * 
 * This header centralizes all window sizing, map dimensions, and coordinate scaling
 * to ensure consistent rendering across different map types and support future
 * window resizing functionality.
 */

// =============================================================================
// WINDOW CONFIGURATION
// =============================================================================

#define WINDOW_DEFAULT_WIDTH    1024
#define WINDOW_DEFAULT_HEIGHT   768
#define WINDOW_TITLE_DEFAULT    "CEngine Game Demo"

// Window aspect ratio for consistent scaling
#define WINDOW_ASPECT_RATIO     ((float)WINDOW_DEFAULT_WIDTH / (float)WINDOW_DEFAULT_HEIGHT)

// =============================================================================
// MAP CONFIGURATION
// =============================================================================

// Default map dimensions (in tiles)
#define MAP_DEFAULT_WIDTH       5
#define MAP_DEFAULT_HEIGHT      5

// Tile sizing for different map types
#define TILE_SIZE_GRID          50.0f   // Square tiles
#define TILE_SIZE_HEX           45.0f   // Hex tiles (slightly smaller to fit better)

// Map padding (world units around the map edges)
#define MAP_PADDING_WORLD       20.0f

// =============================================================================
// COORDINATE SYSTEM CONFIGURATION  
// =============================================================================

// Import coordinate scaling from existing system
#include "coordinate_system.h"

// Derived constants for map bounds calculation
#define MAP_MAX_WIDTH_WORLD     (MAP_DEFAULT_WIDTH * TILE_SIZE_GRID + 2 * MAP_PADDING_WORLD)
#define MAP_MAX_HEIGHT_WORLD    (MAP_DEFAULT_HEIGHT * TILE_SIZE_GRID + 2 * MAP_PADDING_WORLD)

// Hex-specific spatial calculations
#define HEX_WIDTH_FACTOR        1.732f  // sqrt(3) for hex width calculation
#define HEX_HEIGHT_FACTOR       0.75f   // 3/4 for hex height overlap

// =============================================================================
// DISPLAY UTILITY FUNCTIONS
// =============================================================================

// Calculate required world bounds for a given map
static inline float calculate_map_world_width(int map_width, float tile_size, bool is_hex) {
    if (is_hex) {
        return map_width * tile_size * HEX_WIDTH_FACTOR + 2 * MAP_PADDING_WORLD;
    } else {
        return map_width * tile_size + 2 * MAP_PADDING_WORLD;
    }
}

static inline float calculate_map_world_height(int map_height, float tile_size, bool is_hex) {
    if (is_hex) {
        return map_height * tile_size * HEX_HEIGHT_FACTOR + tile_size + 2 * MAP_PADDING_WORLD;
    } else {
        return map_height * tile_size + 2 * MAP_PADDING_WORLD;
    }
}

// Calculate appropriate tile size to fit map in window
static inline float calculate_tile_size_for_window(int map_width, int map_height, 
                                                   int window_width, int window_height, 
                                                   bool is_hex) {
    // Convert window size to world coordinates
    float world_width = (float)window_width / WORLD_COORDINATE_SCALE * 2.0f;  // Full width in world units
    float world_height = (float)window_height / WORLD_COORDINATE_SCALE * 2.0f; // Full height in world units
    
    // Account for padding
    float available_width = world_width - 2 * MAP_PADDING_WORLD;
    float available_height = world_height - 2 * MAP_PADDING_WORLD;
    
    float tile_size_x, tile_size_y;
    
    if (is_hex) {
        tile_size_x = available_width / (map_width * HEX_WIDTH_FACTOR);
        tile_size_y = available_height / (map_height * HEX_HEIGHT_FACTOR + 1.0f);
    } else {
        tile_size_x = available_width / map_width;
        tile_size_y = available_height / map_height;
    }
    
    // Use the smaller dimension to ensure map fits in both directions
    float tile_size = (tile_size_x < tile_size_y) ? tile_size_x : tile_size_y;
    
    // Clamp to reasonable bounds
    if (tile_size < 20.0f) tile_size = 20.0f;
    if (tile_size > 100.0f) tile_size = 100.0f;
    
    return tile_size;
}

// Check if map fits within current world coordinate bounds
static inline bool map_fits_in_world_bounds(int map_width, int map_height, 
                                           float tile_size, bool is_hex) {
    float required_width = calculate_map_world_width(map_width, tile_size, is_hex);
    float required_height = calculate_map_world_height(map_height, tile_size, is_hex);
    
    // Check against world boundary (doubled for full range)
    return (required_width <= WORLD_BOUNDARY_RADIUS * 2.0f) && 
           (required_height <= WORLD_BOUNDARY_RADIUS * 2.0f);
}

#endif // DISPLAY_CONFIG_H