#include "../minunit.h"
#include "game/map_system.h"
#include "core/ecs.h"
#include <stdio.h>

int tests_run = 0;

// Test grid boundary validation
static char* test_grid_boundary_validation() {
    Map map = {0};
    map_init(&map, MAP_GRID, 5, 5, 50.0f);
    
    // Test valid coordinates
    mu_assert("Center should be valid", map_coord_valid(&map, grid_coord(2, 2)));
    mu_assert("Top-left corner should be valid", map_coord_valid(&map, grid_coord(0, 0)));
    mu_assert("Bottom-right corner should be valid", map_coord_valid(&map, grid_coord(4, 4)));
    
    // Test invalid coordinates
    mu_assert("Negative X should be invalid", !map_coord_valid(&map, grid_coord(-1, 2)));
    mu_assert("Negative Y should be invalid", !map_coord_valid(&map, grid_coord(2, -1)));
    mu_assert("Too large X should be invalid", !map_coord_valid(&map, grid_coord(5, 2)));
    mu_assert("Too large Y should be invalid", !map_coord_valid(&map, grid_coord(2, 5)));
    mu_assert("Way outside should be invalid", !map_coord_valid(&map, grid_coord(100, 100)));
    
    map_cleanup(&map);
    return 0;
}

// Test hex boundary validation  
static char* test_hex_boundary_validation() {
    Map map = {0};
    map_init(&map, MAP_HEX_POINTY, 5, 5, 50.0f);
    
    // Test valid hex coordinates - use offset coordinates converted to cube
    mu_assert("Center hex should be valid", map_coord_valid(&map, hex_offset_to_cube((MapCoord){2, 2, 0})));
    mu_assert("Origin hex should be valid", map_coord_valid(&map, hex_offset_to_cube((MapCoord){0, 0, 0})));
    
    // Test coordinates that should map outside our grid
    // Use a coordinate that would fall outside the 5x5 offset grid
    mu_assert("Coordinate outside grid should be invalid", 
              !map_coord_valid(&map, hex_offset_to_cube((MapCoord){5, 2, 0})));
    mu_assert("Negative offset should be invalid", 
              !map_coord_valid(&map, hex_offset_to_cube((MapCoord){-1, 2, 0})));
    
    map_cleanup(&map);
    return 0;
}

// Test that all grid neighbors are within bounds for interior tiles
static char* test_grid_neighbor_bounds() {
    Map map = {0};
    map_init(&map, MAP_GRID, 5, 5, 50.0f);
    
    // Test center tile - all neighbors should be valid
    MapCoord center = grid_coord(2, 2);
    MapCoord neighbors[8];
    int neighbor_count = grid_get_neighbors(center, neighbors, 8);
    
    for (int i = 0; i < neighbor_count; i++) {
        mu_assert("All neighbors of center tile should be valid", 
                  map_coord_valid(&map, neighbors[i]));
    }
    
    map_cleanup(&map);
    return 0;
}

// Test that edge tiles have some invalid neighbors (preventing walk-off)
static char* test_grid_edge_neighbor_bounds() {
    Map map = {0};
    map_init(&map, MAP_GRID, 5, 5, 50.0f);
    
    // Test corner tile - some neighbors should be invalid
    MapCoord corner = grid_coord(0, 0);
    MapCoord neighbors[8];
    int neighbor_count = grid_get_neighbors(corner, neighbors, 8);
    
    int valid_neighbors = 0;
    for (int i = 0; i < neighbor_count; i++) {
        if (map_coord_valid(&map, neighbors[i])) {
            valid_neighbors++;
        }
    }
    
    mu_assert("Corner tile should have fewer than 8 valid neighbors", valid_neighbors < 8);
    mu_assert("Corner tile should have at least 3 valid neighbors", valid_neighbors >= 3);
    
    map_cleanup(&map);
    return 0;
}

// Test hex neighbor validation
static char* test_hex_neighbor_bounds() {
    Map map = {0};
    map_init(&map, MAP_HEX_POINTY, 5, 5, 50.0f);
    
    // Test center hex tile using proper offset->cube conversion
    MapCoord center = hex_offset_to_cube((MapCoord){2, 2, 0}); // Center of 5x5 grid
    MapCoord neighbors[6];
    int neighbor_count = hex_get_neighbors(center, neighbors, 6);
    
    mu_assert("Hex should have exactly 6 neighbors", neighbor_count == 6);
    
    // Check that most neighbors are valid (interior hex)
    int valid_neighbors = 0;
    for (int i = 0; i < neighbor_count; i++) {
        if (map_coord_valid(&map, neighbors[i])) {
            valid_neighbors++;
        }
    }
    
    mu_assert("Interior hex should have most neighbors valid", valid_neighbors >= 4);
    
    map_cleanup(&map);
    return 0;
}

// Test movement validation with blocked terrain
static char* test_movement_blocked_terrain() {
    Map map = {0};
    map_init(&map, MAP_GRID, 5, 5, 50.0f);
    
    // Set up terrain - make a water tile that blocks movement
    MapCoord start = grid_coord(2, 2);
    MapCoord water_tile = grid_coord(2, 3);
    MapCoord passable_tile = grid_coord(3, 2);
    
    map_set_terrain(&map, start, TERRAIN_PLAINS);
    map_set_terrain(&map, water_tile, TERRAIN_WATER);  // Water blocks movement
    map_set_terrain(&map, passable_tile, TERRAIN_ROAD);
    
    // Test movement to blocked terrain
    bool can_move_to_water = map_can_move_to(&map, start, water_tile);
    mu_assert("Should not be able to move to water", !can_move_to_water);
    
    // Test movement to passable terrain
    bool can_move_to_road = map_can_move_to(&map, start, passable_tile);
    mu_assert("Should be able to move to road", can_move_to_road);
    
    map_cleanup(&map);
    return 0;
}

// Test diagonal movement constraints in grid
static char* test_grid_diagonal_movement() {
    Map map = {0};
    map_init(&map, MAP_GRID, 5, 5, 50.0f);
    
    MapCoord start = grid_coord(2, 2);
    
    // Test that all 8 directions are theoretically possible neighbors
    MapCoord neighbors[8];
    int neighbor_count = grid_get_neighbors(start, neighbors, 8);
    mu_assert("Grid should have 8 neighbors", neighbor_count == 8);
    
    // Verify cardinal directions
    bool found_north = false, found_south = false, found_east = false, found_west = false;
    for (int i = 0; i < neighbor_count; i++) {
        if (neighbors[i].x == 2 && neighbors[i].y == 1) found_north = true;
        if (neighbors[i].x == 2 && neighbors[i].y == 3) found_south = true;
        if (neighbors[i].x == 3 && neighbors[i].y == 2) found_east = true;
        if (neighbors[i].x == 1 && neighbors[i].y == 2) found_west = true;
    }
    
    mu_assert("Should find north neighbor", found_north);
    mu_assert("Should find south neighbor", found_south);
    mu_assert("Should find east neighbor", found_east);
    mu_assert("Should find west neighbor", found_west);
    
    map_cleanup(&map);
    return 0;
}

static char* all_tests() {
    mu_test_suite_start();
    
    mu_run_test(test_grid_boundary_validation);
    mu_run_test(test_hex_boundary_validation);
    mu_run_test(test_grid_neighbor_bounds);
    mu_run_test(test_grid_edge_neighbor_bounds);
    mu_run_test(test_hex_neighbor_bounds);
    mu_run_test(test_movement_blocked_terrain);
    mu_run_test(test_grid_diagonal_movement);
    
    return 0;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    char *result = all_tests();
    mu_test_suite_end(result);
    
    return result != 0;
}