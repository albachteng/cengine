#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

#include <stdbool.h>

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

// =============================================================================
// HEX GEOMETRY CALCULATIONS
// =============================================================================

// Calculate actual bounding box for hex maps by checking all hex extents
// For pointy-top hexes: x = size * (sqrt(3) * coord.x + sqrt(3)/2 * coord.y)
//                       y = size * (3/2 * coord.y)
static inline void calculate_hex_map_bounds(int map_width, int map_height, float tile_size,
                                          float* out_min_x, float* out_max_x,
                                          float* out_min_y, float* out_max_y) {
    if (map_width <= 0 || map_height <= 0) {
        *out_min_x = *out_max_x = *out_min_y = *out_max_y = 0.0f;
        return;
    }
    
    // For pointy-top hexes, calculate actual extent by checking all hex boundaries
    float hex_width = 1.732050808f * (tile_size * 0.5f);  // sqrt(3) * radius (flat-to-flat)
    float hex_height_extent = tile_size * 0.5f;            // radius (point-to-point / 2)
    
    float actual_min_x = 1000000.0f, actual_max_x = -1000000.0f;
    float actual_min_y = 1000000.0f, actual_max_y = -1000000.0f;
    
    // Check all hex positions and their extents
    for (int y = 0; y < map_height; y++) {
        for (int x = 0; x < map_width; x++) {
            // Calculate hex center position
            float center_x = tile_size * (1.732050808f * x + 0.866025404f * y);
            float center_y = tile_size * (1.5f * y);
            
            // Calculate hex boundaries
            float hex_left = center_x - hex_width * 0.5f;
            float hex_right = center_x + hex_width * 0.5f;
            float hex_bottom = center_y - hex_height_extent;
            float hex_top = center_y + hex_height_extent;
            
            // Update bounds
            if (hex_left < actual_min_x) actual_min_x = hex_left;
            if (hex_right > actual_max_x) actual_max_x = hex_right;
            if (hex_bottom < actual_min_y) actual_min_y = hex_bottom;
            if (hex_top > actual_max_y) actual_max_y = hex_top;
        }
    }
    
    *out_min_x = actual_min_x;
    *out_max_x = actual_max_x;
    *out_min_y = actual_min_y;
    *out_max_y = actual_max_y;
}

// =============================================================================
// DISPLAY UTILITY FUNCTIONS
// =============================================================================

// Calculate required world bounds for a given map
static inline float calculate_map_world_width(int map_width, float tile_size, bool is_hex) {
    if (is_hex) {
        float min_x, max_x, min_y, max_y;
        calculate_hex_map_bounds(map_width, 1, tile_size, &min_x, &max_x, &min_y, &max_y);
        return (max_x - min_x) + 2 * MAP_PADDING_WORLD;
    } else {
        return map_width * tile_size + 2 * MAP_PADDING_WORLD;
    }
}

static inline float calculate_map_world_height(int map_height, float tile_size, bool is_hex) {
    if (is_hex) {
        float min_x, max_x, min_y, max_y;
        calculate_hex_map_bounds(1, map_height, tile_size, &min_x, &max_x, &min_y, &max_y);
        return (max_y - min_y) + 2 * MAP_PADDING_WORLD;
    } else {
        return map_height * tile_size + 2 * MAP_PADDING_WORLD;
    }
}

// Calculate appropriate tile size to fit map in window (improved hex support)
static inline float calculate_tile_size_for_window(int map_width, int map_height, 
                                                   int window_width, int window_height, 
                                                   bool is_hex) {
    (void)window_width;   // Currently unused - world coordinate system determines available space
    (void)window_height;  // Currently unused - world coordinate system determines available space
    // Convert window size to world coordinates
    // The screen spans from -1 to +1 in normalized coordinates, so full width is 2.0 screen units
    // World coordinates span ±WORLD_BOUNDARY_RADIUS, so available space is 2*WORLD_BOUNDARY_RADIUS
    float world_width = WORLD_BOUNDARY_RADIUS * 2.0f;  // Full width in world units
    float world_height = WORLD_BOUNDARY_RADIUS * 2.0f; // Full height in world units
    
    // Account for padding
    float available_width = world_width - 2 * MAP_PADDING_WORLD;
    float available_height = world_height - 2 * MAP_PADDING_WORLD;
    
    if (!is_hex) {
        // Grid calculation (unchanged)
        float tile_size_x = available_width / map_width;
        float tile_size_y = available_height / map_height;
        float tile_size = (tile_size_x < tile_size_y) ? tile_size_x : tile_size_y;
        
        // Clamp to reasonable bounds
        if (tile_size < 20.0f) tile_size = 20.0f;
        if (tile_size > 100.0f) tile_size = 100.0f;
        return tile_size;
    }
    
    // Hex calculation: find size that maximizes space usage while respecting both constraints
    float best_size = 1.0f;
    
    // Add minimal safety margin to ensure hexes don't touch window edges
    float safety_margin = 1.0f;  // 1 world unit of padding
    float safe_width = available_width - safety_margin;
    float safe_height = available_height - safety_margin;
    
    // Find the largest size that fits both width and height constraints
    // Use a more targeted approach: calculate what each constraint allows
    float size_for_width = 1.0f;
    float size_for_height = 1.0f;
    
    // Binary search for width constraint
    float w_min = 1.0f, w_max = 200.0f;
    for (int i = 0; i < 20; i++) {
        float test_size = (w_min + w_max) * 0.5f;
        float min_x, max_x, min_y, max_y;
        calculate_hex_map_bounds(map_width, map_height, test_size, &min_x, &max_x, &min_y, &max_y);
        
        if ((max_x - min_x) <= safe_width) {
            size_for_width = test_size;
            w_min = test_size;
        } else {
            w_max = test_size;
        }
    }
    
    // Binary search for height constraint  
    float h_min = 1.0f, h_max = 200.0f;
    for (int i = 0; i < 20; i++) {
        float test_size = (h_min + h_max) * 0.5f;
        float min_x, max_x, min_y, max_y;
        calculate_hex_map_bounds(map_width, map_height, test_size, &min_x, &max_x, &min_y, &max_y);
        
        if ((max_y - min_y) <= safe_height) {
            size_for_height = test_size;
            h_min = test_size;
        } else {
            h_max = test_size;
        }
    }
    
    // Take the smaller of the two (the limiting constraint)
    best_size = (size_for_width < size_for_height) ? size_for_width : size_for_height;
    
    // Clamp to reasonable bounds after finding optimal size
    if (best_size < 5.0f) best_size = 5.0f;     // Lower minimum for small windows
    if (best_size > 100.0f) best_size = 100.0f;
    
    return best_size;
}

// Check if map fits within current world coordinate bounds
static inline bool map_fits_in_world_bounds(int map_width, int map_height, 
                                           float tile_size, bool is_hex) {
    if (is_hex) {
        float min_x, max_x, min_y, max_y;
        calculate_hex_map_bounds(map_width, map_height, tile_size, &min_x, &max_x, &min_y, &max_y);
        float required_width = (max_x - min_x) + 2 * MAP_PADDING_WORLD;
        float required_height = (max_y - min_y) + 2 * MAP_PADDING_WORLD;
        
        // Check against world boundary (doubled for full range)
        return (required_width <= WORLD_BOUNDARY_RADIUS * 2.0f) && 
               (required_height <= WORLD_BOUNDARY_RADIUS * 2.0f);
    } else {
        float required_width = calculate_map_world_width(map_width, tile_size, is_hex);
        float required_height = calculate_map_world_height(map_height, tile_size, is_hex);
        
        // Check against world boundary (doubled for full range)
        return (required_width <= WORLD_BOUNDARY_RADIUS * 2.0f) && 
               (required_height <= WORLD_BOUNDARY_RADIUS * 2.0f);
    }
}

#endif // DISPLAY_CONFIG_H