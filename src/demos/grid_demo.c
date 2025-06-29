#include "core/components.h"
#include "core/ecs.h" 
#include "core/input.h"
#include "core/log.h"
#include "core/renderer.h"
#include "core/window.h"
#include "game/map_system.h"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define GRID_DEMO_MAP_WIDTH 3   // Very few tiles for testing
#define GRID_DEMO_MAP_HEIGHT 3
#define GRID_DEMO_TILE_SIZE 60.0f  // Very large tiles to ensure visibility

// Grid demo state
typedef struct {
    Map map;
    Entity player_entity;
    MapCoord player_pos;
    MapType current_map_type;
    bool show_debug;
} GridDemoState;

// Terrain colors for rendering
static Color terrain_colors[TERRAIN_COUNT] = {
    [TERRAIN_PLAINS] = {0.5f, 0.8f, 0.3f, 1.0f},   // Light green
    [TERRAIN_FOREST] = {0.2f, 0.6f, 0.2f, 1.0f},   // Dark green
    [TERRAIN_WATER] = {0.2f, 0.4f, 0.8f, 1.0f},    // Blue
    [TERRAIN_MOUNTAIN] = {0.6f, 0.5f, 0.4f, 1.0f}, // Brown
    [TERRAIN_DESERT] = {0.9f, 0.8f, 0.4f, 1.0f},   // Yellow
    [TERRAIN_SWAMP] = {0.4f, 0.5f, 0.3f, 1.0f},    // Dark yellow-green
    [TERRAIN_ROAD] = {0.7f, 0.7f, 0.7f, 1.0f},     // Light gray
    [TERRAIN_BRIDGE] = {0.8f, 0.6f, 0.4f, 1.0f},   // Light brown
    [TERRAIN_VOID] = {0.1f, 0.1f, 0.1f, 1.0f}      // Dark gray
};

// Generate a simple test map with different terrains
void generate_test_map(Map* map) {
    // Set up some terrain variety
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            MapCoord coord = (map->type == MAP_GRID) ? 
                grid_coord(x, y) : hex_coord(x, y);
            
            TerrainType terrain = TERRAIN_PLAINS; // Default
            
            // Create some patterns
            if (x == 0 || x == map->width - 1 || y == 0 || y == map->height - 1) {
                terrain = TERRAIN_VOID; // Border
            } else if (x == map->width / 2) {
                terrain = TERRAIN_ROAD; // Vertical road
            } else if (y == map->height / 2) {
                terrain = TERRAIN_ROAD; // Horizontal road
            } else if ((x + y) % 5 == 0) {
                terrain = TERRAIN_FOREST;
            } else if ((x * 3 + y * 2) % 7 == 0) {
                terrain = TERRAIN_MOUNTAIN;
            } else if (x > map->width * 0.7f && y < map->height * 0.3f) {
                terrain = TERRAIN_WATER;
            }
            
            map_set_terrain(map, coord, terrain);
        }
    }
}

// Render a single map tile
void render_map_tile(const Map* map, Renderer* renderer, MapCoord coord) {
    const MapNode* node = map_get_node_const(map, coord);
    if (!node) return;
    
    Vec3 world_pos = map_coord_to_world(map, coord);
    Color tile_color = terrain_colors[node->terrain];
    
    // Debug: Print first few tile positions
    static int debug_count = 0;
    if (debug_count < 3) {
        printf("Tile (%d,%d): world=(%.1f,%.1f) screen=(%.3f,%.3f)\\n",
               coord.x, coord.y, world_pos.x, world_pos.y,
               world_pos.x / 120.0f, world_pos.y / 120.0f);
        debug_count++;
    }
    
    // Create a temporary renderable for the tile
    Renderable tile_renderable = {0};
    
    if (map->type == MAP_GRID) {
        tile_renderable.shape = SHAPE_QUAD;
        tile_renderable.data.quad.width = map->tile_size;
        tile_renderable.data.quad.height = map->tile_size;
    } else {
        // For hex, use a circle approximation for now
        tile_renderable.shape = SHAPE_CIRCLE;
        tile_renderable.data.circle.radius = map->tile_size * 0.5f;
    }
    
    tile_renderable.color = tile_color;
    tile_renderable.visible = true;
    
    // Temporarily set transform for rendering
    Transform tile_transform = {0};
    tile_transform.position = world_pos;
    tile_transform.scale = vec3_one();
    
    // Render based on shape
    if (tile_renderable.shape == SHAPE_QUAD) {
        renderer_render_quad(renderer, &tile_transform, &tile_renderable);
    } else {
        renderer_render_circle(renderer, &tile_transform, &tile_renderable);
    }
}

// Render the entire map
void render_map(const Map* map, Renderer* renderer) {
    static int frame_count = 0;
    if (frame_count == 0) {
        printf("Starting map render: %dx%d tiles\\n", map->width, map->height);
    }
    
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            MapCoord coord = (map->type == MAP_GRID) ? 
                grid_coord(x, y) : hex_coord(x, y);
            
            if (map_coord_valid(map, coord)) {
                render_map_tile(map, renderer, coord);
            }
        }
    }
    
    if (frame_count == 0) {
        printf("Finished map render\\n");
    }
    frame_count++;
}

// Render the player
void render_player(GridDemoState* state, Renderer* renderer) {
    Vec3 player_world_pos = map_coord_to_world(&state->map, state->player_pos);
    
    Transform player_transform = {0};
    player_transform.position = player_world_pos;
    player_transform.scale = vec3_one();
    
    Renderable player_renderable = {0};
    player_renderable.shape = SHAPE_CIRCLE;
    player_renderable.data.circle.radius = state->map.tile_size * 0.3f;
    player_renderable.color = (Color){1.0f, 0.0f, 0.0f, 1.0f}; // Red
    player_renderable.visible = true;
    
    renderer_render_circle(renderer, &player_transform, &player_renderable);
}

// Handle player movement input
void handle_player_input(GridDemoState* state, InputState* input) {
    MapCoord new_pos = state->player_pos;
    bool moved = false;
    
    if (state->map.type == MAP_GRID) {
        // Grid movement (4 directions)
        if (input_key_pressed(input, GLFW_KEY_W) || input_key_pressed(input, GLFW_KEY_UP)) {
            new_pos.y -= 1;
            moved = true;
        }
        if (input_key_pressed(input, GLFW_KEY_S) || input_key_pressed(input, GLFW_KEY_DOWN)) {
            new_pos.y += 1;
            moved = true;
        }
        if (input_key_pressed(input, GLFW_KEY_A) || input_key_pressed(input, GLFW_KEY_LEFT)) {
            new_pos.x -= 1;
            moved = true;
        }
        if (input_key_pressed(input, GLFW_KEY_D) || input_key_pressed(input, GLFW_KEY_RIGHT)) {
            new_pos.x += 1;
            moved = true;
        }
    } else {
        // Hex movement (6 directions)
        if (input_key_pressed(input, GLFW_KEY_W)) {
            new_pos.y -= 1; // North
            moved = true;
        }
        if (input_key_pressed(input, GLFW_KEY_S)) {
            new_pos.y += 1; // South
            moved = true;
        }
        if (input_key_pressed(input, GLFW_KEY_Q)) {
            new_pos.x -= 1; // Northwest
            moved = true;
        }
        if (input_key_pressed(input, GLFW_KEY_E)) {
            new_pos.x += 1; // Northeast
            moved = true;
        }
        if (input_key_pressed(input, GLFW_KEY_A)) {
            new_pos.x -= 1;
            new_pos.y += 1; // Southwest
            moved = true;
        }
        if (input_key_pressed(input, GLFW_KEY_D)) {
            new_pos.x += 1;
            new_pos.y -= 1; // Southeast
            moved = true;
        }
    }
    
    // Validate movement
    if (moved && map_coord_valid(&state->map, new_pos)) {
        if (map_can_move_to(&state->map, state->player_pos, new_pos)) {
            // Update occupancy
            map_set_occupant(&state->map, state->player_pos, 0); // Clear old position
            map_set_occupant(&state->map, new_pos, state->player_entity); // Set new position
            state->player_pos = new_pos;
            
            printf("Player moved to (%d, %d) - Terrain: %s (Cost: %d)\\n", 
                   new_pos.x, new_pos.y, 
                   terrain_type_to_string(map_get_node_const(&state->map, new_pos)->terrain),
                   map_get_movement_cost(&state->map, new_pos));
        } else {
            printf("Cannot move to (%d, %d) - blocked!\\n", new_pos.x, new_pos.y);
        }
    }
}

// Switch between map modes
void switch_map_mode(GridDemoState* state) {
    // Clean up current map
    map_cleanup(&state->map);
    
    // Switch mode
    if (state->current_map_type == MAP_GRID) {
        state->current_map_type = MAP_HEX_POINTY;
        printf("Switched to hexagonal map mode\\n");
    } else {
        state->current_map_type = MAP_GRID;
        printf("Switched to grid map mode\\n");
    }
    
    // Initialize new map
    map_init(&state->map, state->current_map_type, 
             GRID_DEMO_MAP_WIDTH, GRID_DEMO_MAP_HEIGHT, GRID_DEMO_TILE_SIZE);
    
    // Center the map in the window
    float map_width_world = GRID_DEMO_MAP_WIDTH * GRID_DEMO_TILE_SIZE;
    float map_height_world = GRID_DEMO_MAP_HEIGHT * GRID_DEMO_TILE_SIZE;
    
    state->map.origin = (Vec3){
        -map_width_world * 0.5f,   // Center horizontally
        -map_height_world * 0.5f,  // Center vertically  
        0.0f
    };
    
    generate_test_map(&state->map);
    
    // Reset player position
    state->player_pos = (state->current_map_type == MAP_GRID) ? 
        grid_coord(GRID_DEMO_MAP_WIDTH / 2, GRID_DEMO_MAP_HEIGHT / 2) :
        hex_coord(GRID_DEMO_MAP_WIDTH / 2, GRID_DEMO_MAP_HEIGHT / 2);
    
    map_set_occupant(&state->map, state->player_pos, state->player_entity);
}

int main(void) {
    // Initialize logging system
    LogConfig log_cfg = {0};
    log_cfg.min_level = LOG_INFO;
    log_init(&log_cfg);
    
    LOG_INFO("Starting Grid Demo");
    
    if (!window_init()) {
        LOG_ERROR("Window init failed");
        return -1;
    }
    
    Window* window = window_create(1024, 768, "Grid Demo - Multi-Modal Map System");
    if (!window) {
        LOG_ERROR("Window creation failed");
        window_terminate();
        return -1;
    }
    
    ECS ecs = {0};
    ecs_init(&ecs);
    
    Renderer renderer = {0};
    if (!renderer_init(&renderer, &ecs)) {
        LOG_ERROR("Renderer initialization failed");
        window_destroy(window);
        window_terminate();
        return -1;
    }
    
    InputState input = {0};
    input_init(&input, window->handle);
    
    // Initialize demo state
    GridDemoState state = {0};
    state.current_map_type = MAP_GRID;
    state.show_debug = false;
    
    // Initialize map
    map_init(&state.map, state.current_map_type, 
             GRID_DEMO_MAP_WIDTH, GRID_DEMO_MAP_HEIGHT, GRID_DEMO_TILE_SIZE);
    
    // Center the map in the window
    // Calculate map size in world units
    float map_width_world = GRID_DEMO_MAP_WIDTH * GRID_DEMO_TILE_SIZE;
    float map_height_world = GRID_DEMO_MAP_HEIGHT * GRID_DEMO_TILE_SIZE;
    
    // Set origin to center the map (assuming screen center is at origin)
    state.map.origin = (Vec3){
        -map_width_world * 0.5f,   // Center horizontally
        -map_height_world * 0.5f,  // Center vertically  
        0.0f
    };
    
    generate_test_map(&state.map);
    
    // Debug: Print map bounds
    printf("Map bounds: origin=(%.1f,%.1f) size=(%.1fx%.1f) tile_size=%.1f\\n",
           state.map.origin.x, state.map.origin.y, 
           map_width_world, map_height_world, GRID_DEMO_TILE_SIZE);
    
    // Create player entity
    state.player_entity = ecs_create_entity(&ecs);
    state.player_pos = grid_coord(GRID_DEMO_MAP_WIDTH / 2, GRID_DEMO_MAP_HEIGHT / 2);
    map_set_occupant(&state.map, state.player_pos, state.player_entity);
    
    LOG_INFO("Grid Demo Initialized!");
    printf("Controls:\\n");
    printf("  Grid Mode: WASD or Arrow Keys to move\\n");
    printf("  Hex Mode: WASD for N/S/NE/SW, Q/E for NW/SE\\n");
    printf("  TAB: Switch between Grid and Hex modes\\n");
    printf("  F1: Toggle debug info\\n");
    printf("  ESC: Exit\\n");
    
    while (!window_should_close(window)) {
        window_poll_events();
        input_update(&input);
        
        // Handle input
        if (input_key_pressed(&input, GLFW_KEY_ESCAPE)) {
            glfwSetWindowShouldClose(window->handle, GLFW_TRUE);
        }
        
        if (input_key_pressed(&input, GLFW_KEY_TAB)) {
            switch_map_mode(&state);
        }
        
        if (input_key_pressed(&input, GLFW_KEY_F1)) {
            state.show_debug = !state.show_debug;
            printf("Debug info: %s\\n", state.show_debug ? "ON" : "OFF");
        }
        
        handle_player_input(&state, &input);
        
        // Render
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        renderer_begin_frame(&renderer);
        
        // Render map
        render_map(&state.map, &renderer);
        
        // Render player
        render_player(&state, &renderer);
        
        // Debug info
        if (state.show_debug) {
            printf("Player: (%d,%d) | Mode: %s | Terrain: %s\\r", 
                   state.player_pos.x, state.player_pos.y,
                   (state.current_map_type == MAP_GRID) ? "Grid" : "Hex",
                   terrain_type_to_string(map_get_node_const(&state.map, state.player_pos)->terrain));
            fflush(stdout);
        }
        
        renderer_end_frame(&renderer);
        window_swap_buffers(window);
    }
    
    // Cleanup
    map_cleanup(&state.map);
    input_cleanup(&input);
    renderer_cleanup(&renderer);
    ecs_cleanup(&ecs);
    window_destroy(window);
    window_terminate();
    
    LOG_INFO("Grid Demo shutdown complete");
    return 0;
}