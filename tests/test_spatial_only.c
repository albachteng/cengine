#include "memory.h"
#include "components.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Copy just the spatial grid types and functions we need
typedef uint32_t Entity;

typedef struct EntityNode {
    Entity entity;
    struct EntityNode* next;
} EntityNode;

typedef struct {
    EntityNode* entities;
} SpatialCell;

typedef struct {
    SpatialCell* cells;
    int grid_width;
    int grid_height;
    float cell_size;
    Vec3 grid_origin;
} SpatialGrid;

// Helper functions
static int spatial_grid_hash(SpatialGrid* grid, int x, int y) {
    if (x < 0 || x >= grid->grid_width || y < 0 || y >= grid->grid_height) {
        return -1;
    }
    return y * grid->grid_width + x;
}

static void spatial_grid_get_cell_coords(SpatialGrid* grid, Vec3 position, int* x, int* y) {
    *x = (int)((position.x - grid->grid_origin.x) / grid->cell_size);
    *y = (int)((position.y - grid->grid_origin.y) / grid->cell_size);
}

void spatial_grid_init_test(SpatialGrid* grid, Arena* arena, Vec3 origin, float width, float height, float cell_size) {
    // ZII: grid should already be zero-initialized
    grid->grid_origin = origin;
    grid->cell_size = cell_size;
    grid->grid_width = (int)(width / cell_size) + 1;
    grid->grid_height = (int)(height / cell_size) + 1;
    
    int total_cells = grid->grid_width * grid->grid_height;
    size_t required_bytes = sizeof(SpatialCell) * total_cells;
    
    printf("Grid dimensions: %dx%d = %d cells\n", grid->grid_width, grid->grid_height, total_cells);
    printf("Required memory: %zu bytes\n", required_bytes);
    
    ArenaStats stats = {0};
    arena_get_stats(arena, &stats);
    printf("Arena before allocation: %zu used, %zu free\n", stats.used_bytes, stats.free_bytes);
    
    grid->cells = (SpatialCell*)arena_alloc(arena, required_bytes);
    
    if (!grid->cells) {
        printf("ERROR: Failed to allocate spatial grid cells\n");
        return;
    }
    
    memset(grid->cells, 0, required_bytes);
    
    arena_get_stats(arena, &stats);
    printf("Arena after allocation: %zu used, %zu free\n", stats.used_bytes, stats.free_bytes);
    printf("Spatial grid initialized successfully\n");
}

void spatial_grid_insert_test(SpatialGrid* grid, Arena* arena, Entity entity, Vec3 position, float radius) {
    if (!grid->cells) {
        printf("ERROR: Grid not initialized\n");
        return;
    }
    
    // Calculate which cells this entity overlaps
    int min_x, min_y, max_x, max_y;
    
    Vec3 min_pos = vec3_create(position.x - radius, position.y - radius, 0.0f);
    Vec3 max_pos = vec3_create(position.x + radius, position.y + radius, 0.0f);
    
    spatial_grid_get_cell_coords(grid, min_pos, &min_x, &min_y);
    spatial_grid_get_cell_coords(grid, max_pos, &max_x, &max_y);
    
    printf("Entity %d at (%.1f, %.1f) radius %.1f covers cells (%d,%d) to (%d,%d)\n",
           entity, position.x, position.y, radius, min_x, min_y, max_x, max_y);
    
    int nodes_created = 0;
    
    // Insert entity into all overlapping cells using arena allocation
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            int cell_index = spatial_grid_hash(grid, x, y);
            if (cell_index >= 0) {
                EntityNode* node = (EntityNode*)arena_alloc(arena, sizeof(EntityNode));
                if (node) {
                    node->entity = entity;
                    node->next = grid->cells[cell_index].entities;
                    grid->cells[cell_index].entities = node;
                    nodes_created++;
                    printf("  Created node in cell (%d,%d) [index %d]\n", x, y, cell_index);
                } else {
                    printf("  ERROR: Failed to allocate node for cell (%d,%d)\n", x, y);
                }
            }
        }
    }
    
    printf("Created %d nodes for entity %d\n", nodes_created, entity);
}

int main() {
    printf("Testing isolated spatial grid...\n");
    
    Arena arena = {0};  // ZII
    if (!arena_init(&arena, 64 * 1024)) {
        printf("Failed to initialize arena\n");
        return -1;
    }
    
    SpatialGrid grid = {0};  // ZII
    
    float boundary_radius = 100.0f;
    float cell_size = 20.0f;
    float grid_size = boundary_radius * 2.2f;  // 220.0f
    Vec3 grid_origin = vec3_create(-grid_size / 2.0f, -grid_size / 2.0f, 0.0f);
    
    printf("Testing with grid size %.1f x %.1f, cell size %.1f\n", grid_size, grid_size, cell_size);
    
    spatial_grid_init_test(&grid, &arena, grid_origin, grid_size, grid_size, cell_size);
    
    if (!grid.cells) {
        printf("Grid initialization failed\n");
        arena_cleanup(&arena);
        return -1;
    }
    
    // Test inserting a few entities
    for (int i = 0; i < 5; i++) {
        Entity entity = i + 1;
        Vec3 position = vec3_create(i * 10.0f - 20.0f, i * 10.0f - 20.0f, 0.0f);
        float radius = 5.0f;
        
        printf("\nInserting entity %d:\n", entity);
        spatial_grid_insert_test(&grid, &arena, entity, position, radius);
    }
    
    ArenaStats final_stats = {0};
    arena_get_stats(&arena, &final_stats);
    printf("\nFinal arena usage: %zu/%zu bytes (%.1f%%)\n", 
           final_stats.used_bytes, final_stats.total_size,
           (float)final_stats.used_bytes / final_stats.total_size * 100.0f);
    
    arena_cleanup(&arena);
    printf("Test completed successfully!\n");
    return 0;
}