#include "../minunit.h"
#include "game/map_system.h"
#include <stdio.h>
#include <stdlib.h>

int tests_run = 0;

static char* test_map_init_cleanup() {
    Map map = {0};
    
    // Test valid initialization
    bool result = map_init(&map, MAP_GRID, 10, 10, 32.0f);
    mu_assert("Map initialization should succeed", result);
    mu_assert("Map type should be set", map.type == MAP_GRID);
    mu_assert("Map width should be set", map.width == 10);
    mu_assert("Map height should be set", map.height == 10);
    mu_assert("Map tile size should be set", map.tile_size == 32.0f);
    mu_assert("Map nodes should be allocated", map.nodes != NULL);
    
    // Test coordinate validation
    mu_assert("Valid coordinate should be valid", map_coord_valid(&map, grid_coord(5, 5)));
    mu_assert("Invalid coordinate (negative) should be invalid", !map_coord_valid(&map, grid_coord(-1, 5)));
    mu_assert("Invalid coordinate (too large) should be invalid", !map_coord_valid(&map, grid_coord(10, 5)));
    
    map_cleanup(&map);
    mu_assert("Map nodes should be freed", map.nodes == NULL);
    
    return 0;
}

static char* test_grid_coordinates() {
    Map map = {0};
    map_init(&map, MAP_GRID, 5, 5, 10.0f);
    
    // Test coordinate creation
    MapCoord coord = grid_coord(2, 3);
    mu_assert("Grid coord x should be correct", coord.x == 2);
    mu_assert("Grid coord y should be correct", coord.y == 3);
    mu_assert("Grid coord z should be 0", coord.z == 0);
    
    // Test coordinate equality
    MapCoord coord2 = grid_coord(2, 3);
    mu_assert("Equal coordinates should be equal", map_coord_equal(coord, coord2));
    
    MapCoord coord3 = grid_coord(2, 4);
    mu_assert("Different coordinates should not be equal", !map_coord_equal(coord, coord3));
    
    // Test world-to-coord conversion
    Vec3 world_pos = {25.0f, 35.0f, 0.0f};
    MapCoord converted = map_world_to_coord(&map, world_pos);
    mu_assert("World to coord x should be correct", converted.x == 2);
    mu_assert("World to coord y should be correct", converted.y == 3);
    
    // Test coord-to-world conversion
    Vec3 world_result = map_coord_to_world(&map, grid_coord(2, 3));
    mu_assert("Coord to world x should be correct", world_result.x == 20.0f);
    mu_assert("Coord to world y should be correct", world_result.y == 30.0f);
    
    map_cleanup(&map);
    return 0;
}

static char* test_hex_coordinates() {
    Map map = {0};
    map_init(&map, MAP_HEX_POINTY, 5, 5, 10.0f);
    
    // Test hex coordinate creation
    MapCoord hex = hex_coord(1, 2);
    mu_assert("Hex coord q should be correct", hex.x == 1);
    mu_assert("Hex coord r should be correct", hex.y == 2);
    mu_assert("Hex coord s should be correct", hex.z == -3); // q + r + s = 0
    
    // Test hex neighbors
    MapCoord neighbors[6];
    int neighbor_count = hex_get_neighbors(hex_coord(0, 0), neighbors, 6);
    mu_assert("Hex should have 6 neighbors", neighbor_count == 6);
    
    // Test hex distance
    MapCoord start = hex_coord(0, 0);
    MapCoord end = hex_coord(2, 1);
    int distance = hex_distance(start, end);
    mu_assert("Hex distance should be calculated correctly", distance == 3);
    
    map_cleanup(&map);
    return 0;
}

static char* test_grid_neighbors() {
    MapCoord neighbors[8];
    int count = grid_get_neighbors(grid_coord(5, 5), neighbors, 8);
    
    mu_assert("Grid should have 8 neighbors", count == 8);
    
    // Check some specific neighbors
    bool found_left = false, found_right = false, found_up = false, found_down = false;
    for (int i = 0; i < count; i++) {
        if (map_coord_equal(neighbors[i], grid_coord(4, 5))) found_left = true;
        if (map_coord_equal(neighbors[i], grid_coord(6, 5))) found_right = true;
        if (map_coord_equal(neighbors[i], grid_coord(5, 4))) found_up = true;
        if (map_coord_equal(neighbors[i], grid_coord(5, 6))) found_down = true;
    }
    
    mu_assert("Should find left neighbor", found_left);
    mu_assert("Should find right neighbor", found_right);
    mu_assert("Should find up neighbor", found_up);
    mu_assert("Should find down neighbor", found_down);
    
    return 0;
}

static char* test_terrain_system() {
    Map map = {0};
    map_init(&map, MAP_GRID, 5, 5, 10.0f);
    
    MapCoord coord = grid_coord(2, 2);
    
    // Test initial terrain (should be plains)
    const MapNode* node = map_get_node_const(&map, coord);
    mu_assert("Initial terrain should be plains", node->terrain == TERRAIN_PLAINS);
    mu_assert("Plains should have movement cost 1", node->movement_cost == 1);
    
    // Test setting terrain
    bool result = map_set_terrain(&map, coord, TERRAIN_FOREST);
    mu_assert("Setting terrain should succeed", result);
    
    node = map_get_node_const(&map, coord);
    mu_assert("Terrain should be updated to forest", node->terrain == TERRAIN_FOREST);
    mu_assert("Forest should have movement cost 2", node->movement_cost == 2);
    mu_assert("Forest should have defense bonus 2", node->defense_bonus == 2);
    
    // Test movement validation
    map_set_terrain(&map, grid_coord(1, 2), TERRAIN_WATER);
    bool can_move = map_can_move_to(&map, coord, grid_coord(1, 2));
    mu_assert("Should not be able to move to water", !can_move);
    
    can_move = map_can_move_to(&map, coord, grid_coord(3, 2));
    mu_assert("Should be able to move to plains", can_move);
    
    map_cleanup(&map);
    return 0;
}

static char* test_occupancy_system() {
    Map map = {0};
    map_init(&map, MAP_GRID, 5, 5, 10.0f);
    
    MapCoord coord1 = grid_coord(1, 1);
    MapCoord coord2 = grid_coord(2, 1);
    
    // Test setting occupant
    bool result = map_set_occupant(&map, coord1, 123); // Entity ID 123
    mu_assert("Setting occupant should succeed", result);
    
    const MapNode* node = map_get_node_const(&map, coord1);
    mu_assert("Occupant should be set", node->occupying_unit == 123);
    
    // Test movement to occupied tile
    map_set_occupant(&map, coord2, 456); // Different entity
    bool can_move = map_can_move_to(&map, coord1, coord2);
    mu_assert("Should not be able to move to occupied tile", !can_move);
    
    // Test movement of same unit
    map_set_occupant(&map, coord1, 456); // Same entity as coord2
    can_move = map_can_move_to(&map, coord1, coord2);
    mu_assert("Same unit should be able to move to its own tile", can_move);
    
    map_cleanup(&map);
    return 0;
}

static char* all_tests() {
    mu_test_suite_start();
    
    mu_run_test(test_map_init_cleanup);
    mu_run_test(test_grid_coordinates);
    mu_run_test(test_hex_coordinates);
    mu_run_test(test_grid_neighbors);
    mu_run_test(test_terrain_system);
    mu_run_test(test_occupancy_system);
    
    return 0;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    char *result = all_tests();
    mu_test_suite_end(result);
    
    return result != 0;
}