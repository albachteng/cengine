#include "game/map_system.h"
#include "core/memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

// Movement cost table for different terrains
static const uint8_t TERRAIN_MOVEMENT_COSTS[TERRAIN_COUNT] = {
    [TERRAIN_PLAINS] = 1,
    [TERRAIN_FOREST] = 2,
    [TERRAIN_WATER] = 0,    // Impassable by default
    [TERRAIN_MOUNTAIN] = 3,
    [TERRAIN_DESERT] = 2,
    [TERRAIN_SWAMP] = 3,
    [TERRAIN_ROAD] = 1,
    [TERRAIN_BRIDGE] = 1,
    [TERRAIN_VOID] = 0      // Impassable
};

// Defense bonus table for different terrains
static const uint8_t TERRAIN_DEFENSE_BONUS[TERRAIN_COUNT] = {
    [TERRAIN_PLAINS] = 0,
    [TERRAIN_FOREST] = 2,
    [TERRAIN_WATER] = 0,
    [TERRAIN_MOUNTAIN] = 3,
    [TERRAIN_DESERT] = 0,
    [TERRAIN_SWAMP] = 1,
    [TERRAIN_ROAD] = 0,
    [TERRAIN_BRIDGE] = 0,
    [TERRAIN_VOID] = 0
};

// Terrain names for debugging
static const char* TERRAIN_NAMES[TERRAIN_COUNT] = {
    [TERRAIN_PLAINS] = "Plains",
    [TERRAIN_FOREST] = "Forest",
    [TERRAIN_WATER] = "Water",
    [TERRAIN_MOUNTAIN] = "Mountain",
    [TERRAIN_DESERT] = "Desert",
    [TERRAIN_SWAMP] = "Swamp",
    [TERRAIN_ROAD] = "Road",
    [TERRAIN_BRIDGE] = "Bridge",
    [TERRAIN_VOID] = "Void"
};

bool map_init(Map* map, MapType type, int width, int height, float tile_size) {
    if (!map || width <= 0 || height <= 0 || tile_size <= 0.0f) {
        return false;
    }
    
    // ZII pattern - initialize with zeros
    *map = (Map){0};
    
    map->type = type;
    map->width = width;
    map->height = height;
    map->tile_size = tile_size;
    map->origin = (Vec3){0.0f, 0.0f, 0.0f};
    
    // Allocate map nodes
    size_t nodes_size = width * height * sizeof(MapNode);
    map->nodes = malloc(nodes_size);
    if (!map->nodes) {
        return false;
    }
    
    // Initialize all nodes to plains with default properties
    for (int i = 0; i < width * height; i++) {
        map->nodes[i] = (MapNode){
            .terrain = TERRAIN_PLAINS,
            .movement_cost = TERRAIN_MOVEMENT_COSTS[TERRAIN_PLAINS],
            .defense_bonus = TERRAIN_DEFENSE_BONUS[TERRAIN_PLAINS],
            .conquerable = true,
            .faction_owner = 0,     // Neutral
            .occupying_unit = 0     // Empty
        };
    }
    
    return true;
}

void map_cleanup(Map* map) {
    if (!map) return;
    
    if (map->nodes) {
        free(map->nodes);
        map->nodes = NULL;
    }
    
    if (map->nav_cache) {
        free(map->nav_cache);
        map->nav_cache = NULL;
    }
    
    // Reset to ZII state
    *map = (Map){0};
}

MapCoord map_world_to_coord(const Map* map, Vec3 world_pos) {
    if (!map) return (MapCoord){0, 0, 0};
    
    // Translate to map-relative coordinates
    Vec3 relative = {
        world_pos.x - map->origin.x,
        world_pos.y - map->origin.y,
        world_pos.z - map->origin.z
    };
    
    switch (map->type) {
        case MAP_GRID: {
            int x = (int)floorf(relative.x / map->tile_size);
            int y = (int)floorf(relative.y / map->tile_size);
            return grid_coord(x, y);
        }
        
        case MAP_HEX_POINTY:
        case MAP_HEX_FLAT: {
            // Convert world position to hex coordinates
            // Using standard hex-to-pixel conversion formulas
            float size = map->tile_size;
            
            if (map->type == MAP_HEX_POINTY) {
                // Pointy-top hexagons
                float q = (sqrtf(3.0f)/3.0f * relative.x - 1.0f/3.0f * relative.y) / size;
                float r = (2.0f/3.0f * relative.y) / size;
                
                // Round to nearest hex
                int qi = (int)roundf(q);
                int ri = (int)roundf(r);
                
                return hex_coord(qi, ri);
            } else {
                // Flat-top hexagons
                float q = (2.0f/3.0f * relative.x) / size;
                float r = (-1.0f/3.0f * relative.x + sqrtf(3.0f)/3.0f * relative.y) / size;
                
                int qi = (int)roundf(q);
                int ri = (int)roundf(r);
                
                return hex_coord(qi, ri);
            }
        }
        
        default:
            return (MapCoord){0, 0, 0};
    }
}

Vec3 map_coord_to_world(const Map* map, MapCoord coord) {
    if (!map) return (Vec3){0, 0, 0};
    
    Vec3 local_pos = {0, 0, 0};
    
    switch (map->type) {
        case MAP_GRID: {
            local_pos.x = coord.x * map->tile_size;
            local_pos.y = coord.y * map->tile_size;
            break;
        }
        
        case MAP_HEX_POINTY: {
            // Pointy-top hexagon pixel coordinates
            float size = map->tile_size;
            local_pos.x = size * (sqrtf(3.0f) * coord.x + sqrtf(3.0f)/2.0f * coord.y);
            local_pos.y = size * (3.0f/2.0f * coord.y);
            break;
        }
        
        case MAP_HEX_FLAT: {
            // Flat-top hexagon pixel coordinates
            float size = map->tile_size;
            local_pos.x = size * (3.0f/2.0f * coord.x);
            local_pos.y = size * (sqrtf(3.0f)/2.0f * coord.x + sqrtf(3.0f) * coord.y);
            break;
        }
        
        default:
            break;
    }
    
    // Translate to world coordinates
    return (Vec3){
        local_pos.x + map->origin.x,
        local_pos.y + map->origin.y,
        local_pos.z + map->origin.z
    };
}

bool map_coord_valid(const Map* map, MapCoord coord) {
    if (!map) return false;
    
    switch (map->type) {
        case MAP_GRID:
            return coord.x >= 0 && coord.x < map->width && 
                   coord.y >= 0 && coord.y < map->height;
                   
        case MAP_HEX_POINTY:
        case MAP_HEX_FLAT: {
            // For hex grids, we restrict to coordinates that directly correspond
            // to our offset storage grid. This prevents players from walking
            // to valid hex coordinates that don't have actual tiles.
            MapCoord offset = hex_cube_to_offset(coord);
            
            // Simple bounds check on the offset coordinates  
            return offset.x >= 0 && offset.x < map->width && 
                   offset.y >= 0 && offset.y < map->height;
        }
        
        default:
            return false;
    }
}

bool map_coord_equal(MapCoord a, MapCoord b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

int map_get_neighbors(const Map* map, MapCoord coord, MapCoord* neighbors, int max_neighbors) {
    if (!map || !neighbors || max_neighbors <= 0) return 0;
    
    switch (map->type) {
        case MAP_GRID:
            return grid_get_neighbors(coord, neighbors, max_neighbors);
            
        case MAP_HEX_POINTY:
        case MAP_HEX_FLAT:
            return hex_get_neighbors(coord, neighbors, max_neighbors);
            
        default:
            return 0;
    }
}

MapNode* map_get_node(Map* map, MapCoord coord) {
    if (!map || !map->nodes || !map_coord_valid(map, coord)) {
        return NULL;
    }
    
    int index;
    
    switch (map->type) {
        case MAP_GRID:
            index = coord.y * map->width + coord.x;
            break;
            
        case MAP_HEX_POINTY:
        case MAP_HEX_FLAT: {
            MapCoord offset = hex_cube_to_offset(coord);
            index = offset.y * map->width + offset.x;
            break;
        }
        
        default:
            return NULL;
    }
    
    return &map->nodes[index];
}

const MapNode* map_get_node_const(const Map* map, MapCoord coord) {
    return map_get_node((Map*)map, coord);
}

bool map_set_terrain(Map* map, MapCoord coord, TerrainType terrain) {
    MapNode* node = map_get_node(map, coord);
    if (!node || terrain >= TERRAIN_COUNT) {
        return false;
    }
    
    node->terrain = terrain;
    node->movement_cost = TERRAIN_MOVEMENT_COSTS[terrain];
    node->defense_bonus = TERRAIN_DEFENSE_BONUS[terrain];
    
    return true;
}

bool map_set_occupant(Map* map, MapCoord coord, Entity unit) {
    MapNode* node = map_get_node(map, coord);
    if (!node) return false;
    
    node->occupying_unit = unit;
    return true;
}

bool map_can_move_to(const Map* map, MapCoord from, MapCoord to) {
    const MapNode* from_node = map_get_node_const(map, from);
    const MapNode* to_node = map_get_node_const(map, to);
    
    if (!from_node || !to_node) return false;
    
    // Can't move to impassable terrain
    if (to_node->movement_cost == 0) return false;
    
    // Can't move to occupied tile (unless it's the same unit)
    if (to_node->occupying_unit != 0 && 
        to_node->occupying_unit != from_node->occupying_unit) {
        return false;
    }
    
    return true;
}

uint8_t map_get_movement_cost(const Map* map, MapCoord coord) {
    const MapNode* node = map_get_node_const(map, coord);
    return node ? node->movement_cost : 0;
}

int map_distance(const Map* map, MapCoord from, MapCoord to) {
    if (!map) return -1;
    
    switch (map->type) {
        case MAP_GRID: {
            // Manhattan distance for grid
            int dx = abs(to.x - from.x);
            int dy = abs(to.y - from.y);
            return dx + dy;
        }
        
        case MAP_HEX_POINTY:
        case MAP_HEX_FLAT:
            return hex_distance(from, to);
            
        default:
            return -1;
    }
}

// Grid coordinate functions
MapCoord grid_coord(int x, int y) {
    return (MapCoord){x, y, 0};
}

int grid_get_neighbors(MapCoord coord, MapCoord* neighbors, int max_neighbors) {
    if (!neighbors || max_neighbors <= 0) return 0;
    
    // 8-directional movement (including diagonals)
    static const int dx[] = {-1, -1, -1,  0,  0,  1,  1,  1};
    static const int dy[] = {-1,  0,  1, -1,  1, -1,  0,  1};
    
    int count = 0;
    for (int i = 0; i < 8 && count < max_neighbors; i++) {
        neighbors[count++] = grid_coord(coord.x + dx[i], coord.y + dy[i]);
    }
    
    return count;
}

// Hex coordinate functions
MapCoord hex_coord(int q, int r) {
    return (MapCoord){q, r, -q - r};  // Cube coordinates: q + r + s = 0
}

MapCoord hex_offset_to_cube(MapCoord offset) {
    // Convert offset coordinates to cube coordinates
    // Using even-r offset (even rows shifted right)
    int q = offset.x - (offset.y - (offset.y & 1)) / 2;
    int r = offset.y;
    return hex_coord(q, r);
}

MapCoord hex_cube_to_offset(MapCoord cube) {
    // Convert cube coordinates to offset coordinates
    int col = cube.x + (cube.y - (cube.y & 1)) / 2;
    int row = cube.y;
    return (MapCoord){col, row, 0};
}

int hex_get_neighbors(MapCoord coord, MapCoord* neighbors, int max_neighbors) {
    if (!neighbors || max_neighbors <= 0) return 0;
    
    // Hex direction vectors in cube coordinates
    static const int dq[] = {1, 0, -1, -1,  0,  1};
    static const int dr[] = {0, 1,  1,  0, -1, -1};
    
    int count = 0;
    for (int i = 0; i < 6 && count < max_neighbors; i++) {
        int q = coord.x + dq[i];
        int r = coord.y + dr[i];
        neighbors[count++] = hex_coord(q, r);
    }
    
    return count;
}

int hex_distance(MapCoord a, MapCoord b) {
    // Hex distance in cube coordinates
    return (abs(a.x - b.x) + abs(a.y - b.y) + abs(a.z - b.z)) / 2;
}

// Debug functions
void map_print_debug(const Map* map) {
    if (!map) return;
    
    printf("Map Debug Information:\n");
    printf("  Type: %d, Size: %dx%d, Tile Size: %.2f\n", 
           map->type, map->width, map->height, map->tile_size);
    printf("  Origin: (%.2f, %.2f, %.2f)\n", 
           map->origin.x, map->origin.y, map->origin.z);
    
    // Print first few tiles
    printf("  First 5x5 tiles:\n");
    for (int y = 0; y < 5 && y < map->height; y++) {
        printf("    ");
        for (int x = 0; x < 5 && x < map->width; x++) {
            MapCoord coord = (map->type == MAP_GRID) ? 
                grid_coord(x, y) : hex_coord(x, y);
            const MapNode* node = map_get_node_const(map, coord);
            if (node) {
                printf("%c ", TERRAIN_NAMES[node->terrain][0]);
            } else {
                printf("? ");
            }
        }
        printf("\n");
    }
}

const char* terrain_type_to_string(TerrainType terrain) {
    if (terrain < TERRAIN_COUNT) {
        return TERRAIN_NAMES[terrain];
    }
    return "Unknown";
}