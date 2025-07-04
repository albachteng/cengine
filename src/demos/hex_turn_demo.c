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
#include <limits.h>

// Demo configuration
#define DEMO_MAP_WIDTH     10
#define DEMO_MAP_HEIGHT    8
#define PLAYER_MAX_HEALTH  100
#define ENEMY_MAX_HEALTH   25
#define MAX_ENEMIES        3

// Demo state
typedef struct {
    Map map;
    TurnManager turn_manager;
    
    // Multiple enemies
    Entity enemy_entities[MAX_ENEMIES];
    int num_enemies;
    int current_enemy_index;  // Which enemy is currently taking their turn
    
    // ECS component types
    ComponentType transform_type;
    ComponentType unit_type;
    
    // Map switching
    MapType current_map_type;
    
    // Visual settings
    bool show_debug;
    float delta_time;
    
    // UI state tracking
    GameState last_displayed_state;
    bool last_waiting_state;
    int last_player_health;
} HexTurnDemoState;

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

// Generate a complex test map that works for both grid and hex
void generate_multi_modal_map(Map* map) {
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            MapCoord coord;
            if (map->type == MAP_GRID) {
                coord = grid_coord(x, y);
            } else {
                // For hex maps, convert offset coordinates to cube coordinates
                coord = hex_offset_to_cube((MapCoord){x, y, 0});
            }
            
            TerrainType terrain = TERRAIN_PLAINS; // Default
            
            // Create terrain patterns that work well for both grid and hex
            if (x == 0 || x == map->width - 1 || y == 0 || y == map->height - 1) {
                terrain = TERRAIN_FOREST; // Border
            }
            // Central river/road pattern
            else if (x == map->width / 2) {
                terrain = (y >= 2 && y <= map->height - 3) ? TERRAIN_WATER : TERRAIN_BRIDGE;
            }
            // Mountain clusters
            else if ((x >= 2 && x <= 4 && y >= 2 && y <= 3) ||
                     (x >= map->width - 5 && x <= map->width - 3 && y >= map->height - 4 && y <= map->height - 2)) {
                terrain = TERRAIN_MOUNTAIN;
            }
            // Desert corner
            else if (x >= map->width - 3 && y >= map->height - 3) {
                terrain = TERRAIN_DESERT;
            }
            // Some forest patches
            else if ((x >= 1 && x <= 2 && y >= map->height - 3 && y <= map->height - 2) ||
                     (x >= map->width - 4 && x <= map->width - 3 && y >= 1 && y <= 2)) {
                terrain = TERRAIN_FOREST;
            }
            // Water pond
            else if (x == 2 && y == map->height - 2) {
                terrain = TERRAIN_WATER;
            }
            // Roads for faster movement
            else if (y == 1 || y == map->height - 2) {
                terrain = TERRAIN_ROAD;
            }
            
            map_set_terrain(map, coord, terrain);
        }
    }
}

// Render a single map tile (supports both grid and hex)
void render_map_tile(const Map* map, Renderer* renderer, MapCoord coord) {
    const MapNode* node = map_get_node_const(map, coord);
    if (!node) return;
    
    Vec3 world_pos = map_coord_to_world(map, coord);
    Color tile_color = terrain_colors[node->terrain];
    
    // Create a temporary renderable for the tile
    Renderable tile_renderable = {0};
    
    if (map->type == MAP_GRID) {
        tile_renderable.shape = SHAPE_QUAD;
        // Make tiles slightly smaller to create visible borders
        float border_size = 2.0f;
        tile_renderable.data.quad.width = map->tile_size - border_size;
        tile_renderable.data.quad.height = map->tile_size - border_size;
    } else {
        // For hex, use a circle approximation
        tile_renderable.shape = SHAPE_CIRCLE;
        tile_renderable.data.circle.radius = (map->tile_size - 2.0f) * 0.45f;
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
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            MapCoord coord;
            if (map->type == MAP_GRID) {
                coord = grid_coord(x, y);
            } else {
                // For hex maps, convert offset coordinates to cube coordinates
                coord = hex_offset_to_cube((MapCoord){x, y, 0});
            }
            
            if (map_coord_valid(map, coord)) {
                render_map_tile(map, renderer, coord);
            }
        }
    }
}

// Render a unit with visual effects
void render_unit(HexTurnDemoState* state, Renderer* renderer, Entity unit_entity) {
    ECS* ecs = renderer->ecs;
    
    Transform* transform = (Transform*)ecs_get_component(ecs, unit_entity, state->transform_type);
    Unit* unit = (Unit*)ecs_get_component(ecs, unit_entity, state->unit_type);
    
    if (!transform || !unit || !unit->is_alive) {
        return;
    }
    
    Transform unit_transform = *transform;
    
    // Background circle (black border)
    Renderable bg_renderable = {0};
    bg_renderable.shape = SHAPE_CIRCLE;
    bg_renderable.data.circle.radius = state->map.tile_size * 0.35f;
    bg_renderable.color = (Color){0.0f, 0.0f, 0.0f, 1.0f}; // Black background
    bg_renderable.visible = true;
    
    renderer_render_circle(renderer, &unit_transform, &bg_renderable);
    
    // Unit circle on top
    Renderable unit_renderable = {0};
    unit_renderable.shape = SHAPE_CIRCLE;
    unit_renderable.data.circle.radius = state->map.tile_size * 0.25f;
    unit_renderable.visible = true;
    
    // Set color based on unit type and damage flash
    if (unit->show_damage_flash) {
        // Flash white when damaged
        unit_renderable.color = (Color){1.0f, 1.0f, 1.0f, 1.0f};
    } else {
        // Different colors for player and enemies
        if (unit->type == UNIT_PLAYER) {
            unit_renderable.color = (Color){0.2f, 1.0f, 0.2f, 1.0f}; // Bright green for player
        } else {
            // Different shades of red for different enemies
            float red_intensity = 0.8f + 0.2f * (unit_entity % 3) / 3.0f;
            unit_renderable.color = (Color){red_intensity, 0.1f, 0.1f, 1.0f}; // Various reds for enemies
        }
    }
    
    renderer_render_circle(renderer, &unit_transform, &unit_renderable);
}

// Handle player movement input (supports both grid and hex)
void handle_player_input(HexTurnDemoState* state, InputState* input, ECS* ecs) {
    // Only allow input during player turn and not waiting for delay
    if (state->turn_manager.current_state != GAME_STATE_PLAYER_TURN || 
        state->turn_manager.waiting_for_delay) return;
    
    // Get player position
    Transform* player_transform = (Transform*)ecs_get_component(ecs, state->turn_manager.player_entity, state->transform_type);
    if (!player_transform) return;
    
    MapCoord current_pos = map_world_to_coord(&state->map, player_transform->position);
    MapCoord target_pos = current_pos;
    bool move_attempted = false;
    
    if (state->map.type == MAP_GRID) {
        // Grid movement (4 directions)
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
    } else {
        // Hex movement (6 directions using cube coordinates)
        if (input_key_pressed(input, GLFW_KEY_S)) {
            // North: decrease r (up in visual grid)
            target_pos = hex_coord(current_pos.x, current_pos.y - 1);
            move_attempted = true;
        } else if (input_key_pressed(input, GLFW_KEY_W)) {
            // South: increase r (down in visual grid)
            target_pos = hex_coord(current_pos.x, current_pos.y + 1);
            move_attempted = true;
        } else if (input_key_pressed(input, GLFW_KEY_Q)) {
            // Northwest: decrease q
            target_pos = hex_coord(current_pos.x - 1, current_pos.y);
            move_attempted = true;
        } else if (input_key_pressed(input, GLFW_KEY_E)) {
            // Northeast: increase q
            target_pos = hex_coord(current_pos.x + 1, current_pos.y);
            move_attempted = true;
        } else if (input_key_pressed(input, GLFW_KEY_A)) {
            // Southwest: decrease q, increase r
            target_pos = hex_coord(current_pos.x - 1, current_pos.y + 1);
            move_attempted = true;
        } else if (input_key_pressed(input, GLFW_KEY_D)) {
            // Southeast: increase q, decrease r
            target_pos = hex_coord(current_pos.x + 1, current_pos.y - 1);
            move_attempted = true;
        }
    }
    
    if (move_attempted) {
        // Try to move/attack
        bool action_taken = turn_manager_try_move_unit(&state->turn_manager, ecs, &state->map,
                                                      state->transform_type, state->unit_type,
                                                      state->turn_manager.player_entity, target_pos);
        
        if (action_taken) {
            // End player turn and start enemy turns
            turn_manager_end_player_turn(&state->turn_manager);
            state->current_enemy_index = 0; // Reset to first enemy
        }
    }
}

// Process multiple enemy turns
void process_enemy_turns(HexTurnDemoState* state, ECS* ecs) {
    if (state->turn_manager.current_state != GAME_STATE_ENEMY_TURN || 
        state->turn_manager.waiting_for_delay) return;
    
    // Check if we have enemies to process
    if (state->current_enemy_index >= state->num_enemies) {
        // All enemies have taken their turn, go back to player
        state->turn_manager.pending_state = GAME_STATE_PLAYER_TURN;
        state->turn_manager.waiting_for_delay = true;
        state->turn_manager.turn_timer = state->turn_manager.turn_delay;
        state->current_enemy_index = 0;
        printf("=== All Enemies Complete - Waiting for Player ===\\n");
        return;
    }
    
    // Process current enemy
    Entity current_enemy = state->enemy_entities[state->current_enemy_index];
    
    // Check if this enemy is still alive
    Unit* enemy_unit = (Unit*)ecs_get_component(ecs, current_enemy, state->unit_type);
    if (!enemy_unit || !enemy_unit->is_alive) {
        // Skip dead enemy, move to next
        state->current_enemy_index++;
        return;
    }
    
    printf("=== Enemy %d Turn ===\\n", state->current_enemy_index + 1);
    
    // Get enemy and player positions
    Transform* enemy_transform = (Transform*)ecs_get_component(ecs, current_enemy, state->transform_type);
    Transform* player_transform = (Transform*)ecs_get_component(ecs, state->turn_manager.player_entity, state->transform_type);
    
    if (!enemy_transform || !player_transform) {
        state->current_enemy_index++;
        return;
    }
    
    MapCoord enemy_pos = map_world_to_coord(&state->map, enemy_transform->position);
    MapCoord player_pos = map_world_to_coord(&state->map, player_transform->position);
    
    // Use the same smart AI as before
    MapCoord best_moves[6]; // Hex has 6 directions, grid has 4
    int max_moves = (state->map.type == MAP_GRID) ? 4 : 6;
    
    // Find best moves for this enemy (adapted for hex if needed)
    int num_best = 0;
    
    if (state->map.type == MAP_GRID) {
        // Use existing grid-based AI
        num_best = get_best_enemy_moves(ecs, &state->map, state->transform_type, state->unit_type,
                                       enemy_pos, player_pos, best_moves, max_moves);
    } else {
        // TODO: Implement hex-specific AI or adapt existing AI
        // For now, use simplified approach
        MapCoord possible_moves[6] = {
            hex_coord(enemy_pos.x, enemy_pos.y - 1),     // North
            hex_coord(enemy_pos.x, enemy_pos.y + 1),     // South
            hex_coord(enemy_pos.x - 1, enemy_pos.y),     // Northwest
            hex_coord(enemy_pos.x + 1, enemy_pos.y),     // Northeast
            hex_coord(enemy_pos.x - 1, enemy_pos.y + 1), // Southwest
            hex_coord(enemy_pos.x + 1, enemy_pos.y - 1)  // Southeast
        };
        
        int best_distance = INT_MAX;
        
        // Find valid moves and best distance
        for (int i = 0; i < 6; i++) {
            if (can_move_to_position(&state->map, possible_moves[i])) {
                int distance = hex_distance(possible_moves[i], player_pos);
                if (distance < best_distance) {
                    best_distance = distance;
                }
            }
        }
        
        // Collect all moves with best distance
        for (int i = 0; i < 6; i++) {
            if (can_move_to_position(&state->map, possible_moves[i])) {
                int distance = hex_distance(possible_moves[i], player_pos);
                if (distance == best_distance) {
                    best_moves[num_best] = possible_moves[i];
                    num_best++;
                }
            }
        }
    }
    
    if (num_best > 0) {
        // Choose randomly among equally good moves
        int choice = rand() % num_best;
        MapCoord target_pos = best_moves[choice];
        
        printf("Enemy %d AI: Found %d equally good moves, chose move to (%d, %d)\\n", 
               state->current_enemy_index + 1, num_best, target_pos.x, target_pos.y);
        
        // Try to move/attack
        turn_manager_try_move_unit(&state->turn_manager, ecs, &state->map,
                                  state->transform_type, state->unit_type,
                                  current_enemy, target_pos);
    } else {
        printf("Enemy %d AI: No valid moves available, skipping turn\\n", state->current_enemy_index + 1);
    }
    
    // Move to next enemy after a delay
    state->current_enemy_index++;
    state->turn_manager.waiting_for_delay = true;
    state->turn_manager.turn_timer = state->turn_manager.turn_delay * 0.5f; // Shorter delay between enemies
}

// Switch between map modes
void switch_map_mode(HexTurnDemoState* state, ECS* ecs) {
    // Save current unit positions in offset coordinates for conversion
    MapCoord player_offset, enemy_offsets[MAX_ENEMIES];
    
    // Convert current positions to offset coordinates
    Transform* player_transform = (Transform*)ecs_get_component(ecs, state->turn_manager.player_entity, state->transform_type);
    if (player_transform) {
        MapCoord current_pos = map_world_to_coord(&state->map, player_transform->position);
        if (state->map.type == MAP_HEX_POINTY) {
            player_offset = hex_cube_to_offset(current_pos);
        } else {
            player_offset = current_pos;
        }
    }
    
    for (int i = 0; i < state->num_enemies; i++) {
        Transform* enemy_transform = (Transform*)ecs_get_component(ecs, state->enemy_entities[i], state->transform_type);
        if (enemy_transform) {
            MapCoord current_pos = map_world_to_coord(&state->map, enemy_transform->position);
            if (state->map.type == MAP_HEX_POINTY) {
                enemy_offsets[i] = hex_cube_to_offset(current_pos);
            } else {
                enemy_offsets[i] = current_pos;
            }
        }
    }
    
    // Clean up current map
    map_cleanup(&state->map);
    
    // Switch mode and determine appropriate tile size
    float tile_size;
    bool is_hex;
    
    if (state->current_map_type == MAP_GRID) {
        state->current_map_type = MAP_HEX_POINTY;
        is_hex = true;
        printf("\\nSwitched to hexagonal map mode\\n");
        printf("Controls: W/S=N/S, Q/E=NW/NE, A/D=SW/SE (6 directions)\\n");
    } else {
        state->current_map_type = MAP_GRID;
        is_hex = false;
        printf("\\nSwitched to grid map mode\\n");
        printf("Controls: WASD or Arrow Keys (4 directions)\\n");
    }
    
    // Calculate appropriate tile size for current window
    tile_size = calculate_tile_size_for_window(
        DEMO_MAP_WIDTH, DEMO_MAP_HEIGHT, WINDOW_DEFAULT_WIDTH,
        WINDOW_DEFAULT_HEIGHT, is_hex);
    
    // Initialize new map
    map_init(&state->map, state->current_map_type, DEMO_MAP_WIDTH, DEMO_MAP_HEIGHT, tile_size);
    
    // Calculate actual map bounds for centering
    float map_width_world = calculate_map_world_width(DEMO_MAP_WIDTH, tile_size, is_hex);
    float map_height_world = calculate_map_world_height(DEMO_MAP_HEIGHT, tile_size, is_hex);
    
    // Center the map in the world coordinate system
    state->map.origin = (Vec3){
        -map_width_world * 0.5f + MAP_PADDING_WORLD,
        -map_height_world * 0.5f + MAP_PADDING_WORLD,
        0.0f
    };
    
    generate_multi_modal_map(&state->map);
    
    // Convert unit positions to new coordinate system and update
    MapCoord new_player_pos;
    if (state->current_map_type == MAP_HEX_POINTY) {
        new_player_pos = hex_offset_to_cube(player_offset);
    } else {
        new_player_pos = player_offset;
    }
    
    // Update player position
    if (map_coord_valid(&state->map, new_player_pos)) {
        player_transform->position = map_coord_to_world(&state->map, new_player_pos);
        map_set_occupant(&state->map, new_player_pos, state->turn_manager.player_entity);
    }
    
    // Update enemy positions
    for (int i = 0; i < state->num_enemies; i++) {
        MapCoord new_enemy_pos;
        if (state->current_map_type == MAP_HEX_POINTY) {
            new_enemy_pos = hex_offset_to_cube(enemy_offsets[i]);
        } else {
            new_enemy_pos = enemy_offsets[i];
        }
        
        Transform* enemy_transform = (Transform*)ecs_get_component(ecs, state->enemy_entities[i], state->transform_type);
        if (enemy_transform && map_coord_valid(&state->map, new_enemy_pos)) {
            enemy_transform->position = map_coord_to_world(&state->map, new_enemy_pos);
            map_set_occupant(&state->map, new_enemy_pos, state->enemy_entities[i]);
        }
    }
}

// Render UI information
void render_ui(HexTurnDemoState* state, ECS* ecs) {
    Unit* player_unit = (Unit*)ecs_get_component(ecs, state->turn_manager.player_entity, state->unit_type);
    
    int current_player_health = player_unit ? player_unit->current_health : 0;
    int alive_enemies = 0;
    
    // Count alive enemies
    for (int i = 0; i < state->num_enemies; i++) {
        Unit* enemy_unit = (Unit*)ecs_get_component(ecs, state->enemy_entities[i], state->unit_type);
        if (enemy_unit && enemy_unit->is_alive) {
            alive_enemies++;
        }
    }
    
    // Only update UI if something changed
    if (state->turn_manager.current_state != state->last_displayed_state ||
        state->turn_manager.waiting_for_delay != state->last_waiting_state ||
        current_player_health != state->last_player_health) {
        
        // Create status string
        const char* status;
        if (state->turn_manager.waiting_for_delay) {
            if (state->turn_manager.pending_state == GAME_STATE_PLAYER_TURN) {
                status = "Waiting for Player...";
            } else {
                status = "Enemy Turn...";
            }
        } else {
            status = game_state_to_string(state->turn_manager.current_state);
        }
        
        // Print health and turn info to console
        printf("\\r[%s] Player: %d HP | Enemies: %d alive | Mode: %s\\n", 
               status,
               current_player_health,
               alive_enemies,
               (state->current_map_type == MAP_GRID) ? "Grid" : "Hex");
        
        // Update tracking state
        state->last_displayed_state = state->turn_manager.current_state;
        state->last_waiting_state = state->turn_manager.waiting_for_delay;
        state->last_player_health = current_player_health;
    }
}

int main(void) {
    // Initialize logging system
    LogConfig log_cfg = {0};
    log_cfg.min_level = LOG_INFO;
    log_init(&log_cfg);
    
    LOG_INFO("Starting Hex Turn-Based Demo");
    
    if (!window_init()) {
        LOG_ERROR("Window init failed");
        return -1;
    }
    
    Window* window = window_create(WINDOW_DEFAULT_WIDTH, WINDOW_DEFAULT_HEIGHT, 
                                   "Hex Turn-Based Demo - Multi-Modal Combat");
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
    HexTurnDemoState state = {0};
    state.show_debug = false;
    state.current_map_type = MAP_GRID; // Start with grid
    state.num_enemies = MAX_ENEMIES;
    state.current_enemy_index = 0;
    
    // Initialize UI tracking (use invalid values to force first display)
    state.last_displayed_state = GAME_STATE_COUNT; // Invalid state
    state.last_waiting_state = false;
    state.last_player_health = -1;
    
    // Register component types
    state.transform_type = ecs_register_component(&ecs, sizeof(Transform));
    if (!unit_system_init(&ecs, &state.unit_type)) {
        LOG_ERROR("Unit system initialization failed");
        return -1;
    }
    
    // Calculate appropriate tile size for grid mode
    float tile_size = calculate_tile_size_for_window(DEMO_MAP_WIDTH, DEMO_MAP_HEIGHT,
                                                    WINDOW_DEFAULT_WIDTH, WINDOW_DEFAULT_HEIGHT, false);
    
    // Initialize map
    map_init(&state.map, state.current_map_type, DEMO_MAP_WIDTH, DEMO_MAP_HEIGHT, tile_size);
    
    // Center the map
    float map_width_world = calculate_map_world_width(DEMO_MAP_WIDTH, tile_size, false);
    float map_height_world = calculate_map_world_height(DEMO_MAP_HEIGHT, tile_size, false);
    
    state.map.origin = (Vec3){
        -map_width_world * 0.5f + MAP_PADDING_WORLD,
        -map_height_world * 0.5f + MAP_PADDING_WORLD,
        0.0f
    };
    
    generate_multi_modal_map(&state.map);
    
    // Initialize turn manager
    turn_manager_init(&state.turn_manager);
    
    // Create player at bottom-left
    MapCoord player_start = grid_coord(2, 2);
    state.turn_manager.player_entity = unit_create(&ecs, state.transform_type, state.unit_type, 
                                                  UNIT_PLAYER, player_start, PLAYER_MAX_HEALTH);
    
    Transform* player_transform = (Transform*)ecs_get_component(&ecs, state.turn_manager.player_entity, state.transform_type);
    player_transform->position = map_coord_to_world(&state.map, player_start);
    player_transform->scale = vec3_one();
    map_set_occupant(&state.map, player_start, state.turn_manager.player_entity);
    
    // Create multiple enemies at different positions
    MapCoord enemy_positions[MAX_ENEMIES] = {
        grid_coord(DEMO_MAP_WIDTH - 3, DEMO_MAP_HEIGHT - 3),  // Top-right
        grid_coord(2, DEMO_MAP_HEIGHT - 3),                   // Top-left
        grid_coord(DEMO_MAP_WIDTH - 3, 2)                     // Bottom-right
    };
    
    for (int i = 0; i < state.num_enemies; i++) {
        state.enemy_entities[i] = unit_create(&ecs, state.transform_type, state.unit_type, 
                                             UNIT_ENEMY, enemy_positions[i], ENEMY_MAX_HEALTH);
        
        Transform* enemy_transform = (Transform*)ecs_get_component(&ecs, state.enemy_entities[i], state.transform_type);
        enemy_transform->position = map_coord_to_world(&state.map, enemy_positions[i]);
        enemy_transform->scale = vec3_one();
        map_set_occupant(&state.map, enemy_positions[i], state.enemy_entities[i]);
        
        printf("Created Enemy %d at (%d, %d)\\n", i + 1, enemy_positions[i].x, enemy_positions[i].y);
    }
    
    // Set the first enemy as the main enemy for turn manager (backwards compatibility)
    state.turn_manager.enemy_entity = state.enemy_entities[0];
    
    LOG_INFO("Hex Turn-Based Demo Initialized!");
    printf("\\nControls:\\n");
    printf("  Grid Mode: WASD or Arrow Keys (4 directions)\\n");
    printf("  Hex Mode: W/S=N/S, Q/E=NW/NE, A/D=SW/SE (6 directions)\\n");
    printf("  TAB: Switch between Grid and Hex modes\\n");
    printf("  F1: Toggle debug info\\n");
    printf("  ESC: Exit\\n");
    printf("\\nGame: Player (Green) vs %d Enemies (Red)\\n", state.num_enemies);
    printf("Move into enemy to attack! Water blocks movement.\\n\\n");
    
    // Force initial UI display
    render_ui(&state, &ecs);
    
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
            switch_map_mode(&state, &ecs);
        }
        
        // Handle player input (only during player turn)
        handle_player_input(&state, &input, &ecs);
        
        // Process enemy turns (multiple enemies)
        process_enemy_turns(&state, &ecs);
        
        // Update systems
        turn_manager_update(&state.turn_manager, state.delta_time);
        
        // Update unit visual effects
        Unit* player_unit = (Unit*)ecs_get_component(&ecs, state.turn_manager.player_entity, state.unit_type);
        if (player_unit) unit_update_visual_effects(player_unit, state.delta_time);
        
        for (int i = 0; i < state.num_enemies; i++) {
            Unit* enemy_unit = (Unit*)ecs_get_component(&ecs, state.enemy_entities[i], state.unit_type);
            if (enemy_unit) unit_update_visual_effects(enemy_unit, state.delta_time);
        }
        
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
        
        // Render all units
        render_unit(&state, &renderer, state.turn_manager.player_entity);
        for (int i = 0; i < state.num_enemies; i++) {
            render_unit(&state, &renderer, state.enemy_entities[i]);
        }
        
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
    
    LOG_INFO("Hex Turn-Based Demo shutdown complete");
    return 0;
}