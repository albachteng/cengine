#include "game/unit_system.h"
#include "game/map_system.h"
#include "core/components.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <limits.h>

// Constants
#define DAMAGE_FLASH_DURATION 0.5f
#define DEFAULT_ATTACK_DAMAGE 5
#define DEFAULT_TURN_DELAY 1.0f

bool unit_system_init(ECS* ecs, ComponentType* unit_type) {
    if (!ecs || !unit_type) return false;
    
    *unit_type = ecs_register_component(ecs, sizeof(Unit));
    return true;
}

Entity unit_create(ECS* ecs, ComponentType transform_type, ComponentType unit_type, 
                  UnitType unit_type_enum, MapCoord position, int max_health) {
    (void)position; // Position will be set by caller
    
    if (!ecs) return 0;
    
    Entity entity = ecs_create_entity(ecs);
    if (entity == 0) return 0;
    
    // Add transform component
    Transform* transform = (Transform*)ecs_add_component(ecs, entity, transform_type);
    if (!transform) {
        ecs_destroy_entity(ecs, entity);
        return 0;
    }
    
    // Position will be set by the caller using map coordinate conversion
    *transform = (Transform){0};
    
    // Add unit component
    Unit* unit = (Unit*)ecs_add_component(ecs, entity, unit_type);
    if (!unit) {
        ecs_destroy_entity(ecs, entity);
        return 0;
    }
    
    // Initialize unit data
    unit->type = unit_type_enum;
    unit->max_health = max_health;
    unit->current_health = max_health;
    unit->is_alive = true;
    unit->damage_flash_timer = 0.0f;
    unit->show_damage_flash = false;
    
    return entity;
}

bool unit_is_alive(const Unit* unit) {
    return unit && unit->is_alive && unit->current_health > 0;
}

void unit_take_damage(Unit* unit, int damage) {
    if (!unit || !unit->is_alive) return;
    
    unit->current_health -= damage;
    if (unit->current_health <= 0) {
        unit->current_health = 0;
        unit->is_alive = false;
    }
    
    // Start damage flash effect
    unit->damage_flash_timer = DAMAGE_FLASH_DURATION;
    unit->show_damage_flash = true;
    
    printf("Unit took %d damage! Health: %d/%d %s\n", 
           damage, unit->current_health, unit->max_health,
           unit->is_alive ? "" : "(DEAD)");
}

void unit_update_visual_effects(Unit* unit, float delta_time) {
    if (!unit) return;
    
    // Update damage flash timer
    if (unit->damage_flash_timer > 0.0f) {
        unit->damage_flash_timer -= delta_time;
        if (unit->damage_flash_timer <= 0.0f) {
            unit->damage_flash_timer = 0.0f;
            unit->show_damage_flash = false;
        }
    }
}

void turn_manager_init(TurnManager* manager) {
    if (!manager) return;
    
    *manager = (TurnManager){0};
    manager->current_state = GAME_STATE_PLAYER_TURN;
    manager->pending_state = GAME_STATE_PLAYER_TURN;
    manager->attack_damage = DEFAULT_ATTACK_DAMAGE;
    manager->flash_duration = DAMAGE_FLASH_DURATION;
    manager->turn_delay = DEFAULT_TURN_DELAY;
    manager->turn_timer = 0.0f;
    manager->waiting_for_delay = false;
    
    // Initialize random seed for AI decision making
    srand((unsigned int)time(NULL));
}

bool can_move_to_position(Map* map, MapCoord position) {
    if (!map_coord_valid(map, position)) {
        return false;
    }
    
    const MapNode* node = map_get_node_const(map, position);
    if (!node) return false;
    
    // Only allow movement on non-water terrain
    return node->terrain != TERRAIN_WATER;
}

Entity get_unit_at_position(ECS* ecs, Map* map, ComponentType transform_type, 
                           ComponentType unit_type, MapCoord position) {
    (void)transform_type; // Unused parameter
    
    if (!ecs || !map) return 0;
    
    // Check map occupancy first (faster)
    const MapNode* node = map_get_node_const(map, position);
    if (!node || node->occupying_unit == 0) return 0;
    
    // Verify the occupant is actually a unit
    if (!ecs_has_component(ecs, node->occupying_unit, unit_type)) {
        return 0;
    }
    
    return node->occupying_unit;
}

void perform_attack(Unit* attacker, Unit* defender, int damage) {
    if (!attacker || !defender || !attacker->is_alive || !defender->is_alive) {
        return;
    }
    
    printf("Attack! %s attacks %s\n", 
           (attacker->type == UNIT_PLAYER) ? "Player" : "Enemy",
           (defender->type == UNIT_PLAYER) ? "Player" : "Enemy");
    
    unit_take_damage(defender, damage);
}

bool turn_manager_try_move_unit(TurnManager* manager, ECS* ecs, Map* map, 
                               ComponentType transform_type, ComponentType unit_type,
                               Entity unit_entity, MapCoord target_position) {
    if (!manager || !ecs || !map) return false;
    
    // Get unit components
    Transform* transform = (Transform*)ecs_get_component(ecs, unit_entity, transform_type);
    Unit* unit = (Unit*)ecs_get_component(ecs, unit_entity, unit_type);
    
    if (!transform || !unit || !unit->is_alive) return false;
    
    // Get current position
    MapCoord current_pos = map_world_to_coord(map, transform->position);
    
    // Check if target position is valid for movement
    if (!can_move_to_position(map, target_position)) {
        printf("Cannot move to (%d, %d) - invalid terrain or position\n", 
               target_position.x, target_position.y);
        return false;
    }
    
    // Check if there's a unit at the target position
    Entity target_unit = get_unit_at_position(ecs, map, transform_type, unit_type, target_position);
    
    if (target_unit != 0) {
        // There's a unit at the target - this is an attack!
        Unit* target_unit_comp = (Unit*)ecs_get_component(ecs, target_unit, unit_type);
        if (target_unit_comp && target_unit_comp->is_alive) {
            perform_attack(unit, target_unit_comp, manager->attack_damage);
            return true; // Attack successful, counts as a turn
        }
    }
    
    // No unit at target - this is a move
    // Update map occupancy
    map_set_occupant(map, current_pos, 0); // Clear old position
    map_set_occupant(map, target_position, unit_entity); // Set new position
    
    // Update transform position
    Vec3 new_world_pos = map_coord_to_world(map, target_position);
    transform->position = new_world_pos;
    
    printf("Unit moved to (%d, %d)\n", target_position.x, target_position.y);
    return true;
}

void turn_manager_end_player_turn(TurnManager* manager) {
    if (!manager || manager->current_state != GAME_STATE_PLAYER_TURN) return;
    
    // Start turn delay
    manager->pending_state = GAME_STATE_ENEMY_TURN;
    manager->waiting_for_delay = true;
    manager->turn_timer = manager->turn_delay;
    
    printf("=== Player Turn Complete - Waiting for Enemy ===\n");
}

// Helper function to find the best available moves for enemy AI
int get_best_enemy_moves(ECS* ecs, Map* map, ComponentType transform_type, ComponentType unit_type,
                        MapCoord enemy_pos, MapCoord player_pos, MapCoord* best_moves, int max_moves) {
    if (!ecs || !map || !best_moves || max_moves < 4) return 0;
    
    // All possible adjacent moves (4-directional)
    MapCoord possible_moves[4] = {
        {enemy_pos.x, enemy_pos.y + 1, 0}, // Up
        {enemy_pos.x, enemy_pos.y - 1, 0}, // Down
        {enemy_pos.x - 1, enemy_pos.y, 0}, // Left
        {enemy_pos.x + 1, enemy_pos.y, 0}  // Right
    };
    
    typedef struct {
        MapCoord coord;
        int distance;
        bool valid;
    } MoveOption;
    
    MoveOption options[4] = {0};
    int valid_count = 0;
    int best_distance = INT_MAX;
    
    // Evaluate each possible move
    for (int i = 0; i < 4; i++) {
        MapCoord move = possible_moves[i];
        options[i].coord = move;
        options[i].valid = false;
        
        // Check if move is valid (terrain and boundaries)
        if (!can_move_to_position(map, move)) {
            continue;
        }
        
        // Note: We don't need to check for units at target since attacks are also valid moves
        
        // Calculate Manhattan distance to player after this move
        int distance = abs(move.x - player_pos.x) + abs(move.y - player_pos.y);
        
        options[i].distance = distance;
        options[i].valid = true;
        valid_count++;
        
        // Track the best distance
        if (distance < best_distance) {
            best_distance = distance;
        }
    }
    
    // If no valid moves, return 0
    if (valid_count == 0) {
        return 0;
    }
    
    // Collect all moves that tie for the best distance
    int best_count = 0;
    for (int i = 0; i < 4; i++) {
        if (options[i].valid && options[i].distance == best_distance) {
            best_moves[best_count] = options[i].coord;
            best_count++;
        }
    }
    
    return best_count;
}

void turn_manager_process_enemy_turn(TurnManager* manager, ECS* ecs, Map* map,
                                   ComponentType transform_type, ComponentType unit_type) {
    if (!manager || !ecs || !map || manager->current_state != GAME_STATE_ENEMY_TURN) return;
    
    // Check if enemy is still alive
    Unit* enemy_unit = (Unit*)ecs_get_component(ecs, manager->enemy_entity, unit_type);
    if (!enemy_unit || !enemy_unit->is_alive) {
        manager->current_state = GAME_STATE_GAME_OVER;
        printf("=== Game Over - Player Wins! ===\n");
        return;
    }
    
    // Get enemy and player positions
    Transform* enemy_transform = (Transform*)ecs_get_component(ecs, manager->enemy_entity, transform_type);
    Transform* player_transform = (Transform*)ecs_get_component(ecs, manager->player_entity, transform_type);
    
    if (!enemy_transform || !player_transform) {
        manager->current_state = GAME_STATE_PLAYER_TURN;
        return;
    }
    
    MapCoord enemy_pos = map_world_to_coord(map, enemy_transform->position);
    MapCoord player_pos = map_world_to_coord(map, player_transform->position);
    
    // Smart AI: find best available moves and choose randomly among ties
    MapCoord best_moves[4];
    int num_best = get_best_enemy_moves(ecs, map, transform_type, unit_type, 
                                       enemy_pos, player_pos, best_moves, 4);
    
    if (num_best > 0) {
        // Choose randomly among equally good moves
        int choice = rand() % num_best;
        MapCoord target_pos = best_moves[choice];
        
        printf("Enemy AI: Found %d equally good moves, chose move to (%d, %d)\n", 
               num_best, target_pos.x, target_pos.y);
        
        // Try to move/attack
        turn_manager_try_move_unit(manager, ecs, map, transform_type, unit_type, 
                                  manager->enemy_entity, target_pos);
    } else {
        printf("Enemy AI: No valid moves available, skipping turn\n");
    }
    
    // End enemy turn with delay
    manager->pending_state = GAME_STATE_PLAYER_TURN;
    manager->waiting_for_delay = true;
    manager->turn_timer = manager->turn_delay;
    
    printf("=== Enemy Turn Complete - Waiting for Player ===\n");
}

void turn_manager_update(TurnManager* manager, float delta_time) {
    if (!manager) return;
    
    // Handle turn delay
    if (manager->waiting_for_delay) {
        manager->turn_timer -= delta_time;
        if (manager->turn_timer <= 0.0f) {
            // Delay complete - transition to pending state
            manager->current_state = manager->pending_state;
            manager->waiting_for_delay = false;
            manager->turn_timer = 0.0f;
            
            // Announce new turn
            if (manager->current_state == GAME_STATE_PLAYER_TURN) {
                printf("=== Player Turn ===\n");
            } else if (manager->current_state == GAME_STATE_ENEMY_TURN) {
                printf("=== Enemy Turn ===\n");
            }
        }
    }
}

bool is_game_over(TurnManager* manager, ECS* ecs, ComponentType unit_type) {
    if (!manager || !ecs) return false;
    
    if (manager->current_state == GAME_STATE_GAME_OVER) return true;
    
    // Check if player is dead
    Unit* player_unit = (Unit*)ecs_get_component(ecs, manager->player_entity, unit_type);
    if (!player_unit || !player_unit->is_alive) {
        manager->current_state = GAME_STATE_GAME_OVER;
        printf("=== Game Over - Enemy Wins! ===\n");
        return true;
    }
    
    // Check if enemy is dead
    Unit* enemy_unit = (Unit*)ecs_get_component(ecs, manager->enemy_entity, unit_type);
    if (!enemy_unit || !enemy_unit->is_alive) {
        manager->current_state = GAME_STATE_GAME_OVER;
        printf("=== Game Over - Player Wins! ===\n");
        return true;
    }
    
    return false;
}

const char* game_state_to_string(GameState state) {
    switch (state) {
        case GAME_STATE_PLAYER_TURN: return "Player Turn";
        case GAME_STATE_ENEMY_TURN: return "Enemy Turn";
        case GAME_STATE_GAME_OVER: return "Game Over";
        default: return "Unknown";
    }
}