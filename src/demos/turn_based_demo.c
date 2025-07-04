#include "core/components.h"
#include "core/display_config.h"
#include "core/ecs.h"
#include "core/input.h"
#include "core/log.h"
#include "core/renderer.h"
#include "core/window.h"
#include "game/map_system.h"
#include "game/unit_system.h"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Demo configuration
#define DEMO_MAP_WIDTH     10
#define DEMO_MAP_HEIGHT    8
#define PLAYER_MAX_HEALTH  100
#define ENEMY_MAX_HEALTH   30

// Demo state
typedef struct {
    Map map;
    TurnManager turn_manager;
    
    // ECS component types
    ComponentType transform_type;
    ComponentType unit_type;
    
    // Visual settings
    bool show_debug;
    float delta_time;
    
    // Window resize support
    bool need_map_resize;
    ECS* ecs;  // Store reference to ECS for resize callback
    
    // UI state tracking
    GameState last_displayed_state;
    bool last_waiting_state;
    int last_player_health;
    int last_enemy_health;
} TurnBasedDemoState;

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

// Generate a more complex test map for turn-based demo
void generate_demo_map(Map* map) {
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            MapCoord coord = grid_coord(x, y);
            TerrainType terrain = TERRAIN_PLAINS; // Default
            
            // Create a more interesting map with varied terrain
            if (x == 0 || x == map->width - 1 || y == 0 || y == map->height - 1) {
                terrain = TERRAIN_FOREST; // Forest border
            } 
            // Water river running through the middle
            else if (x == map->width / 2 && y >= 2 && y <= map->height - 3) {
                terrain = TERRAIN_WATER;
            }
            // Bridge across the river
            else if (x == map->width / 2 && y == map->height / 2) {
                terrain = TERRAIN_BRIDGE;
            }
            // Mountain clusters
            else if ((x >= 3 && x <= 5 && y >= 2 && y <= 4) ||
                     (x >= map->width - 6 && x <= map->width - 4 && y >= map->height - 5 && y <= map->height - 3)) {
                terrain = TERRAIN_MOUNTAIN;
            }
            // Water ponds
            else if ((x == 2 && y == map->height - 2) || 
                     (x == map->width - 3 && y == 2)) {
                terrain = TERRAIN_WATER;
            }
            // Roads connecting key areas
            else if ((y == 1 && x >= 1 && x <= map->width - 2) ||
                     (y == map->height - 2 && x >= 1 && x <= map->width - 2) ||
                     (x == 1 && y >= 1 && y <= map->height - 2) ||
                     (x == map->width - 2 && y >= 1 && y <= map->height - 2)) {
                terrain = TERRAIN_ROAD;
            }
            // Desert areas
            else if (x >= map->width - 4 && y >= map->height - 4) {
                terrain = TERRAIN_DESERT;
            }
            // Some forest patches
            else if ((x >= 1 && x <= 3 && y >= map->height / 2 + 1 && y <= map->height / 2 + 2) ||
                     (x >= map->width - 4 && x <= map->width - 2 && y >= 3 && y <= 5)) {
                terrain = TERRAIN_FOREST;
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
    
    // Create a temporary renderable for the tile
    Renderable tile_renderable = {0};
    tile_renderable.shape = SHAPE_QUAD;
    
    // Make tiles slightly smaller to create visible borders
    float border_size = 2.0f;
    tile_renderable.data.quad.width = map->tile_size - border_size;
    tile_renderable.data.quad.height = map->tile_size - border_size;
    tile_renderable.color = tile_color;
    tile_renderable.visible = true;
    
    // Temporarily set transform for rendering
    Transform tile_transform = {0};
    tile_transform.position = world_pos;
    tile_transform.scale = vec3_one();
    
    renderer_render_quad(renderer, &tile_transform, &tile_renderable);
}

// Render the entire map
void render_map(const Map* map, Renderer* renderer) {
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            MapCoord coord = grid_coord(x, y);
            if (map_coord_valid(map, coord)) {
                render_map_tile(map, renderer, coord);
            }
        }
    }
}

// Render a unit with visual effects
void render_unit(TurnBasedDemoState* state, Renderer* renderer, Entity unit_entity) {
    static int debug_count = 0;
    ECS* ecs = renderer->ecs;
    
    Transform* transform = (Transform*)ecs_get_component(ecs, unit_entity, state->transform_type);
    Unit* unit = (Unit*)ecs_get_component(ecs, unit_entity, state->unit_type);
    
    if (debug_count < 5) {
        printf("render_unit called for entity %u - transform: %p, unit: %p\\n", 
               unit_entity, (void*)transform, (void*)unit);
        if (unit) {
            printf("  Unit alive: %s, type: %d\\n", unit->is_alive ? "yes" : "no", unit->type);
        }
        if (transform) {
            printf("  Transform position: (%.2f, %.2f, %.2f)\\n", 
                   transform->position.x, transform->position.y, transform->position.z);
            printf("  Transform scale: (%.2f, %.2f, %.2f)\\n", 
                   transform->scale.x, transform->scale.y, transform->scale.z);
        }
        debug_count++;
    }
    
    if (!transform || !unit || !unit->is_alive) {
        if (debug_count < 5) {
            printf("  SKIPPING RENDER - missing components or dead unit\\n");
        }
        return;
    }
    
    Transform unit_transform = *transform;
    
    // First render a background indicator (black circle)
    Renderable bg_renderable = {0};
    bg_renderable.shape = SHAPE_CIRCLE;
    bg_renderable.data.circle.radius = state->map.tile_size * 0.4f; // Smaller
    bg_renderable.color = (Color){0.0f, 0.0f, 0.0f, 1.0f}; // Black background
    bg_renderable.visible = true;
    
    if (debug_count < 5) {
        printf("  Rendering bg circle with radius: %.2f (world units)\\n", bg_renderable.data.circle.radius);
        printf("  Screen radius will be: %.4f\\n", bg_renderable.data.circle.radius / 120.0f);
    }
    
    renderer_render_circle(renderer, &unit_transform, &bg_renderable);
    
    // Then render the unit on top
    Renderable unit_renderable = {0};
    unit_renderable.shape = SHAPE_CIRCLE;
    unit_renderable.data.circle.radius = state->map.tile_size * 0.3f; // Smaller
    unit_renderable.visible = true;
    
    // Set color based on unit type and damage flash
    if (unit->show_damage_flash) {
        // Flash white when damaged
        unit_renderable.color = (Color){1.0f, 1.0f, 1.0f, 1.0f};
    } else {
        // Normal colors - make them more distinct
        if (unit->type == UNIT_PLAYER) {
            unit_renderable.color = (Color){0.2f, 1.0f, 0.2f, 1.0f}; // Bright green for player
        } else {
            unit_renderable.color = (Color){1.0f, 0.2f, 0.2f, 1.0f}; // Bright red for enemy
        }
    }
    
    renderer_render_circle(renderer, &unit_transform, &unit_renderable);
}

// Handle player movement input
void handle_player_input(TurnBasedDemoState* state, InputState* input, ECS* ecs) {
    // Only allow input during player turn and not waiting for delay
    if (state->turn_manager.current_state != GAME_STATE_PLAYER_TURN || 
        state->turn_manager.waiting_for_delay) return;
    
    // Get player position
    Transform* player_transform = (Transform*)ecs_get_component(ecs, state->turn_manager.player_entity, state->transform_type);
    if (!player_transform) return;
    
    MapCoord current_pos = map_world_to_coord(&state->map, player_transform->position);
    MapCoord target_pos = current_pos;
    bool move_attempted = false;
    
    // Grid movement
    if (input_key_pressed(input, GLFW_KEY_W) || input_key_pressed(input, GLFW_KEY_UP)) {
        target_pos.y += 1;
        move_attempted = true;
    } else if (input_key_pressed(input, GLFW_KEY_S) || input_key_pressed(input, GLFW_KEY_DOWN)) {
        target_pos.y -= 1;
        move_attempted = true;
    } else if (input_key_pressed(input, GLFW_KEY_A) || input_key_pressed(input, GLFW_KEY_LEFT)) {
        target_pos.x -= 1;
        move_attempted = true;
    } else if (input_key_pressed(input, GLFW_KEY_D) || input_key_pressed(input, GLFW_KEY_RIGHT)) {
        target_pos.x += 1;
        move_attempted = true;
    }
    
    if (move_attempted) {
        // Try to move/attack
        bool action_taken = turn_manager_try_move_unit(&state->turn_manager, ecs, &state->map,
                                                      state->transform_type, state->unit_type,
                                                      state->turn_manager.player_entity, target_pos);
        
        if (action_taken) {
            // End player turn and start enemy turn
            turn_manager_end_player_turn(&state->turn_manager);
        }
    }
}

// Window resize callback
void on_window_resize(int width, int height, void* user_data) {
    TurnBasedDemoState* state = (TurnBasedDemoState*)user_data;
    
    printf("Window resized to %dx%d - recalculating map layout...\\n", width, height);
    
    // Calculate new tile size to fit the map in the new window size
    float new_tile_size = calculate_tile_size_for_window(DEMO_MAP_WIDTH, DEMO_MAP_HEIGHT,
                                                        width, height, false);
    
    // Update map tile size
    state->map.tile_size = new_tile_size;
    
    // Recenter the map
    float map_width_world = calculate_map_world_width(DEMO_MAP_WIDTH, new_tile_size, false);
    float map_height_world = calculate_map_world_height(DEMO_MAP_HEIGHT, new_tile_size, false);
    
    state->map.origin = (Vec3){
        -map_width_world * 0.5f + MAP_PADDING_WORLD,
        -map_height_world * 0.5f + MAP_PADDING_WORLD,
        0.0f
    };
    
    // Update unit positions to match new map layout
    if (state->turn_manager.player_entity != 0) {
        Transform* player_transform = (Transform*)ecs_get_component(state->ecs, state->turn_manager.player_entity, state->transform_type);
        if (player_transform) {
            MapCoord player_coord = map_world_to_coord(&state->map, player_transform->position);
            player_transform->position = map_coord_to_world(&state->map, player_coord);
        }
    }
    
    if (state->turn_manager.enemy_entity != 0) {
        Transform* enemy_transform = (Transform*)ecs_get_component(state->ecs, state->turn_manager.enemy_entity, state->transform_type);
        if (enemy_transform) {
            MapCoord enemy_coord = map_world_to_coord(&state->map, enemy_transform->position);
            enemy_transform->position = map_coord_to_world(&state->map, enemy_coord);
        }
    }
    
    printf("New tile size: %.1f, Map origin: (%.1f, %.1f)\\n", 
           new_tile_size, state->map.origin.x, state->map.origin.y);
}

// Render UI information (only when state changes)
void render_ui(TurnBasedDemoState* state, ECS* ecs) {
    Unit* player_unit = (Unit*)ecs_get_component(ecs, state->turn_manager.player_entity, state->unit_type);
    Unit* enemy_unit = (Unit*)ecs_get_component(ecs, state->turn_manager.enemy_entity, state->unit_type);
    
    int current_player_health = player_unit ? player_unit->current_health : 0;
    int current_enemy_health = enemy_unit ? enemy_unit->current_health : 0;
    
    // Only update UI if something changed
    if (state->turn_manager.current_state != state->last_displayed_state ||
        state->turn_manager.waiting_for_delay != state->last_waiting_state ||
        current_player_health != state->last_player_health ||
        current_enemy_health != state->last_enemy_health) {
        
        // Create status string
        const char* status;
        if (state->turn_manager.waiting_for_delay) {
            status = state->turn_manager.pending_state == GAME_STATE_PLAYER_TURN ? 
                     "Waiting for Player..." : "Waiting for Enemy...";
        } else {
            status = game_state_to_string(state->turn_manager.current_state);
        }
        
        // Print health and turn info to console
        printf("\\r[%s] Player: %d/%d HP | Enemy: %d/%d HP\\n", 
               status,
               current_player_health,
               player_unit ? player_unit->max_health : 0,
               current_enemy_health,
               enemy_unit ? enemy_unit->max_health : 0);
        
        // Update tracking state
        state->last_displayed_state = state->turn_manager.current_state;
        state->last_waiting_state = state->turn_manager.waiting_for_delay;
        state->last_player_health = current_player_health;
        state->last_enemy_health = current_enemy_health;
    }
}

int main(void) {
    // Initialize logging system
    LogConfig log_cfg = {0};
    log_cfg.min_level = LOG_INFO;
    log_init(&log_cfg);
    
    LOG_INFO("Starting Turn-Based Demo");
    
    if (!window_init()) {
        LOG_ERROR("Window init failed");
        return -1;
    }
    
    Window* window = window_create(WINDOW_DEFAULT_WIDTH, WINDOW_DEFAULT_HEIGHT, 
                                   "Turn-Based Demo - Grid Combat");
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
    TurnBasedDemoState state = {0};
    state.show_debug = false;
    state.ecs = &ecs;  // Store ECS reference for resize callback
    
    // Initialize UI tracking (use invalid values to force first display)
    state.last_displayed_state = GAME_STATE_COUNT; // Invalid state
    state.last_waiting_state = false;
    state.last_player_health = -1;
    state.last_enemy_health = -1;
    
    // Register component types
    state.transform_type = ecs_register_component(&ecs, sizeof(Transform));
    if (!unit_system_init(&ecs, &state.unit_type)) {
        LOG_ERROR("Unit system initialization failed");
        return -1;
    }
    
    // Calculate appropriate tile size
    float tile_size = calculate_tile_size_for_window(DEMO_MAP_WIDTH, DEMO_MAP_HEIGHT,
                                                    WINDOW_DEFAULT_WIDTH, WINDOW_DEFAULT_HEIGHT, false);
    
    // Initialize map
    map_init(&state.map, MAP_GRID, DEMO_MAP_WIDTH, DEMO_MAP_HEIGHT, tile_size);
    
    // Center the map
    float map_width_world = calculate_map_world_width(DEMO_MAP_WIDTH, tile_size, false);
    float map_height_world = calculate_map_world_height(DEMO_MAP_HEIGHT, tile_size, false);
    
    state.map.origin = (Vec3){
        -map_width_world * 0.5f + MAP_PADDING_WORLD,
        -map_height_world * 0.5f + MAP_PADDING_WORLD,
        0.0f
    };
    
    generate_demo_map(&state.map);
    
    // Initialize turn manager
    turn_manager_init(&state.turn_manager);
    
    // Create player at bottom-left (safe starting position)
    MapCoord player_start = grid_coord(2, 2);
    state.turn_manager.player_entity = unit_create(&ecs, state.transform_type, state.unit_type, 
                                                  UNIT_PLAYER, player_start, PLAYER_MAX_HEALTH);
    
    Transform* player_transform = (Transform*)ecs_get_component(&ecs, state.turn_manager.player_entity, state.transform_type);
    player_transform->position = map_coord_to_world(&state.map, player_start);
    player_transform->scale = vec3_one(); // Fix: Set the scale!
    map_set_occupant(&state.map, player_start, state.turn_manager.player_entity);
    
    // Create enemy at top-right (safe starting position)
    MapCoord enemy_start = grid_coord(DEMO_MAP_WIDTH - 3, DEMO_MAP_HEIGHT - 3);
    state.turn_manager.enemy_entity = unit_create(&ecs, state.transform_type, state.unit_type, 
                                                 UNIT_ENEMY, enemy_start, ENEMY_MAX_HEALTH);
    
    Transform* enemy_transform = (Transform*)ecs_get_component(&ecs, state.turn_manager.enemy_entity, state.transform_type);
    enemy_transform->position = map_coord_to_world(&state.map, enemy_start);
    enemy_transform->scale = vec3_one(); // Fix: Set the scale!
    map_set_occupant(&state.map, enemy_start, state.turn_manager.enemy_entity);
    
    // Set up window resize callback
    window_set_resize_callback(window, on_window_resize, &state);
    
    LOG_INFO("Turn-Based Demo Initialized!");
    printf("\\nControls:\\n");
    printf("  WASD or Arrow Keys: Move/Attack\\n");
    printf("  F1: Toggle debug info\\n");
    printf("  ESC: Exit\\n");
    printf("\\nResize the window to test dynamic map scaling!\\n");
    printf("\\nGame: Player (Green) vs Enemy (Red)\\n");
    printf("Move into enemy to attack! Water blocks movement.\\n\\n");
    
    // Force initial UI display
    render_ui(&state, &ecs);
    
    // Debug: Print unit positions
    printf("\\nDEBUG: Unit Positions\\n");
    printf("Player entity: %u\\n", state.turn_manager.player_entity);
    printf("Enemy entity: %u\\n", state.turn_manager.enemy_entity);
    
    Transform* player_debug_transform = (Transform*)ecs_get_component(&ecs, state.turn_manager.player_entity, state.transform_type);
    Transform* enemy_debug_transform = (Transform*)ecs_get_component(&ecs, state.turn_manager.enemy_entity, state.transform_type);
    
    if (player_debug_transform) {
        printf("Player world position: (%.2f, %.2f, %.2f)\\n", 
               player_debug_transform->position.x, player_debug_transform->position.y, player_debug_transform->position.z);
        MapCoord player_coord = map_world_to_coord(&state.map, player_debug_transform->position);
        printf("Player map coordinate: (%d, %d)\\n", player_coord.x, player_coord.y);
    } else {
        printf("ERROR: Player transform component not found!\\n");
    }
    
    if (enemy_debug_transform) {
        printf("Enemy world position: (%.2f, %.2f, %.2f)\\n", 
               enemy_debug_transform->position.x, enemy_debug_transform->position.y, enemy_debug_transform->position.z);
        MapCoord enemy_coord = map_world_to_coord(&state.map, enemy_debug_transform->position);
        printf("Enemy map coordinate: (%d, %d)\\n", enemy_coord.x, enemy_coord.y);
    } else {
        printf("ERROR: Enemy transform component not found!\\n");
    }
    
    printf("Map origin: (%.2f, %.2f)\\n", state.map.origin.x, state.map.origin.y);
    printf("Map tile size: %.2f\\n", state.map.tile_size);
    printf("\\n");
    
    float last_time = (float)glfwGetTime();
    
    while (!window_should_close(window)) {
        float current_time = (float)glfwGetTime();
        state.delta_time = current_time - last_time;
        last_time = current_time;
        
        window_poll_events();
        
        // Handle input
        if (input_key_pressed(&input, GLFW_KEY_ESCAPE)) {
            glfwSetWindowShouldClose(window->handle, GLFW_TRUE);
        }
        
        if (input_key_pressed(&input, GLFW_KEY_F1)) {
            state.show_debug = !state.show_debug;
            printf("Debug info: %s\\n", state.show_debug ? "ON" : "OFF");
        }
        
        if (input_key_pressed(&input, GLFW_KEY_TAB)) {
            printf("\\nTAB pressed - but turn_based_demo doesn't support map switching.\\n");
            printf("Use 'make run-hexturn' for map switching support!\\n");
        }
        
        // Handle player input (only during player turn)
        handle_player_input(&state, &input, &ecs);
        
        // Process enemy turn (only when not waiting for delay)
        if (state.turn_manager.current_state == GAME_STATE_ENEMY_TURN && 
            !state.turn_manager.waiting_for_delay) {
            turn_manager_process_enemy_turn(&state.turn_manager, &ecs, &state.map,
                                          state.transform_type, state.unit_type);
        }
        
        // Update systems
        turn_manager_update(&state.turn_manager, state.delta_time);
        
        // Update unit visual effects
        Unit* player_unit = (Unit*)ecs_get_component(&ecs, state.turn_manager.player_entity, state.unit_type);
        Unit* enemy_unit = (Unit*)ecs_get_component(&ecs, state.turn_manager.enemy_entity, state.unit_type);
        
        if (player_unit) unit_update_visual_effects(player_unit, state.delta_time);
        if (enemy_unit) unit_update_visual_effects(enemy_unit, state.delta_time);
        
        // Check for game over
        if (is_game_over(&state.turn_manager, &ecs, state.unit_type)) {
            // Game over - could show menu or restart
        }
        
        // Clear input state for next frame
        input_update(&input);
        
        // Render
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        renderer_begin_frame(&renderer);
        
        // Render map
        render_map(&state.map, &renderer);
        
        // Render units
        render_unit(&state, &renderer, state.turn_manager.player_entity);
        render_unit(&state, &renderer, state.turn_manager.enemy_entity);
        
        // Render UI
        render_ui(&state, &ecs);
        
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
    
    LOG_INFO("Turn-Based Demo shutdown complete");
    return 0;
}