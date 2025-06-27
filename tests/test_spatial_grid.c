#include "memory.h"
#include "physics.h"
#include "components.h"
#include <stdio.h>
#include <assert.h>

int main() {
    printf("Testing spatial grid with arena...\n");
    
    Arena arena = {0};  // ZII
    if (!arena_init(&arena, 64 * 1024)) {
        printf("Failed to initialize arena\n");
        return -1;
    }
    
    ArenaStats stats = {0};
    arena_get_stats(&arena, &stats);
    printf("Arena initialized: %zu bytes total\n", stats.total_size);
    
    SpatialGrid grid = {0};  // ZII
    
    float boundary_radius = 100.0f;
    float cell_size = 20.0f;
    float grid_size = boundary_radius * 2.2f;  // 220.0f
    Vec3 grid_origin = vec3_create(-grid_size / 2.0f, -grid_size / 2.0f, 0.0f);
    
    printf("Grid parameters:\n");
    printf("  Origin: (%.1f, %.1f, %.1f)\n", grid_origin.x, grid_origin.y, grid_origin.z);
    printf("  Size: %.1f x %.1f\n", grid_size, grid_size);
    printf("  Cell size: %.1f\n", cell_size);
    
    // Calculate expected grid dimensions
    int expected_width = (int)(grid_size / cell_size) + 1;
    int expected_height = (int)(grid_size / cell_size) + 1;
    int expected_cells = expected_width * expected_height;
    size_t expected_memory = expected_cells * sizeof(SpatialCell);
    
    printf("Expected grid: %dx%d = %d cells (%zu bytes)\n", 
           expected_width, expected_height, expected_cells, expected_memory);
    
    if (expected_memory > stats.total_size) {
        printf("ERROR: Grid would require %zu bytes but arena only has %zu bytes\n",
               expected_memory, stats.total_size);
        arena_cleanup(&arena);
        return -1;
    }
    
    spatial_grid_init(&grid, &arena, grid_origin, grid_size, grid_size, cell_size);
    
    if (!grid.cells) {
        printf("ERROR: Grid cells not allocated\n");
        arena_cleanup(&arena);
        return -1;
    }
    
    arena_get_stats(&arena, &stats);
    printf("After grid init: %zu bytes used, %zu bytes free\n", 
           stats.used_bytes, stats.free_bytes);
    
    // Test basic grid operations
    printf("Testing grid operations...\n");
    
    spatial_grid_clear(&grid);
    printf("Grid cleared successfully\n");
    
    // Test entity insertion
    Entity test_entity = 42;
    Vec3 test_position = vec3_create(0.0f, 0.0f, 0.0f);
    float test_radius = 5.0f;
    
    printf("Inserting entity %d at (%.1f, %.1f, %.1f) with radius %.1f\n",
           test_entity, test_position.x, test_position.y, test_position.z, test_radius);
    
    spatial_grid_insert(&grid, &arena, test_entity, test_position, test_radius);
    
    arena_get_stats(&arena, &stats);
    printf("After entity insert: %zu bytes used, %zu bytes free\n", 
           stats.used_bytes, stats.free_bytes);
    
    printf("Spatial grid test completed successfully!\n");
    
    arena_cleanup(&arena);
    return 0;
}