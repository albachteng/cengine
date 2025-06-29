#ifndef MAP_SYSTEM_H
#define MAP_SYSTEM_H

#include "core/components.h"
#include "core/ecs.h"
#include <stdint.h>
#include <stdbool.h>

// Map coordinate systems
typedef enum {
    MAP_GRID,        // Square grid (4-directional + diagonals)
    MAP_HEX_POINTY,  // Hexagonal grid with pointy tops
    MAP_HEX_FLAT,    // Hexagonal grid with flat tops
    MAP_VORONOI,     // Voronoi regions (future)
    MAP_CUSTOM       // Custom polygonal regions (future)
} MapType;

// Generic coordinate for different map systems
typedef struct {
    int x, y;        // Primary coordinates
    int z;           // Third coordinate for hex systems (cube coordinates)
} MapCoord;

// Terrain types for map tiles
typedef enum {
    TERRAIN_PLAINS,
    TERRAIN_FOREST,
    TERRAIN_WATER,
    TERRAIN_MOUNTAIN,
    TERRAIN_DESERT,
    TERRAIN_SWAMP,
    TERRAIN_ROAD,
    TERRAIN_BRIDGE,
    TERRAIN_VOID,     // Impassable/non-existent
    TERRAIN_COUNT
} TerrainType;

// Properties for each map tile/node
typedef struct {
    TerrainType terrain;
    uint8_t movement_cost;    // Cost to enter this tile (0 = impassable)
    uint8_t defense_bonus;    // Defensive bonus for units on this tile
    bool conquerable;         // Can this tile be captured
    Entity faction_owner;     // Which faction controls this tile (0 = neutral)
    Entity occupying_unit;    // Unit currently on this tile (0 = empty)
} MapNode;

// Map data structure
typedef struct {
    MapType type;
    int width, height;        // Map dimensions
    MapNode* nodes;           // Tile data (width * height array)
    
    // Rendering properties
    float tile_size;          // Size of each tile in world units
    Vec3 origin;              // World position of map origin
    
    // Navigation cache (for pathfinding optimization)
    void* nav_cache;          // Opaque pathfinding cache data
} Map;

// Grid position component for entities
typedef struct {
    MapCoord coord;           // Map coordinates
    bool is_moving;           // True if entity is transitioning between tiles
    MapCoord target_coord;    // Target coordinate for movement
    float move_progress;      // Movement animation progress (0.0 - 1.0)
} GridPosition;

// Map system functions
bool map_init(Map* map, MapType type, int width, int height, float tile_size);
void map_cleanup(Map* map);

// Coordinate conversion and utilities
MapCoord map_world_to_coord(const Map* map, Vec3 world_pos);
Vec3 map_coord_to_world(const Map* map, MapCoord coord);
bool map_coord_valid(const Map* map, MapCoord coord);
bool map_coord_equal(MapCoord a, MapCoord b);

// Neighbor finding (returns number of neighbors found)
int map_get_neighbors(const Map* map, MapCoord coord, MapCoord* neighbors, int max_neighbors);

// Tile access and modification
MapNode* map_get_node(Map* map, MapCoord coord);
const MapNode* map_get_node_const(const Map* map, MapCoord coord);
bool map_set_terrain(Map* map, MapCoord coord, TerrainType terrain);
bool map_set_occupant(Map* map, MapCoord coord, Entity unit);

// Movement validation
bool map_can_move_to(const Map* map, MapCoord from, MapCoord to);
uint8_t map_get_movement_cost(const Map* map, MapCoord coord);

// Line of sight and distance
bool map_has_line_of_sight(const Map* map, MapCoord from, MapCoord to);
int map_distance(const Map* map, MapCoord from, MapCoord to);

// Coordinate system specific functions
// Grid coordinates (square grid)
MapCoord grid_coord(int x, int y);
int grid_get_neighbors(MapCoord coord, MapCoord* neighbors, int max_neighbors);

// Hex coordinates (using offset coordinates for storage, cube for calculations)
MapCoord hex_coord(int q, int r);
MapCoord hex_offset_to_cube(MapCoord offset);
MapCoord hex_cube_to_offset(MapCoord cube);
int hex_get_neighbors(MapCoord coord, MapCoord* neighbors, int max_neighbors);
int hex_distance(MapCoord a, MapCoord b);

// Debug and visualization
void map_print_debug(const Map* map);
const char* terrain_type_to_string(TerrainType terrain);

#endif // MAP_SYSTEM_H