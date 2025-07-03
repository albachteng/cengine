#ifndef UNIT_SYSTEM_H
#define UNIT_SYSTEM_H

#include "core/ecs.h"
#include "map_system.h"

// Unit types
typedef enum {
    UNIT_PLAYER,
    UNIT_ENEMY,
    UNIT_TYPE_COUNT
} UnitType;

// Unit component - contains unit-specific data
typedef struct {
    UnitType type;
    int max_health;
    int current_health;
    bool is_alive;
    
    // Visual feedback for damage
    float damage_flash_timer;
    bool show_damage_flash;
} Unit;

// Game state for turn management
typedef enum {
    GAME_STATE_PLAYER_TURN,
    GAME_STATE_ENEMY_TURN,
    GAME_STATE_GAME_OVER,
    GAME_STATE_COUNT
} GameState;

// Turn-based game manager
typedef struct {
    GameState current_state;
    GameState pending_state;  // State to transition to after delay
    Entity player_entity;
    Entity enemy_entity;
    
    // Game rules
    int attack_damage;
    
    // Turn timing
    float turn_delay;         // Delay between turns in seconds
    float turn_timer;         // Current turn delay timer
    bool waiting_for_delay;   // Whether we're in a turn delay
    
    // Visual feedback
    float flash_duration;
    float flash_timer;
} TurnManager;

// Unit management functions
bool unit_system_init(ECS* ecs, ComponentType* unit_type);
Entity unit_create(ECS* ecs, ComponentType transform_type, ComponentType unit_type, 
                  UnitType unit_type_enum, MapCoord position, int max_health);
bool unit_is_alive(const Unit* unit);
void unit_take_damage(Unit* unit, int damage);
void unit_update_visual_effects(Unit* unit, float delta_time);

// Turn management functions
void turn_manager_init(TurnManager* manager);
bool turn_manager_try_move_unit(TurnManager* manager, ECS* ecs, Map* map, 
                               ComponentType transform_type, ComponentType unit_type,
                               Entity unit_entity, MapCoord target_position);
void turn_manager_end_player_turn(TurnManager* manager);
void turn_manager_process_enemy_turn(TurnManager* manager, ECS* ecs, Map* map,
                                   ComponentType transform_type, ComponentType unit_type);
void turn_manager_update(TurnManager* manager, float delta_time);

// Combat functions
bool can_move_to_position(Map* map, MapCoord position);
Entity get_unit_at_position(ECS* ecs, Map* map, ComponentType transform_type, 
                           ComponentType unit_type, MapCoord position);
void perform_attack(Unit* attacker, Unit* defender, int damage);

// Game state queries
bool is_game_over(TurnManager* manager, ECS* ecs, ComponentType unit_type);
const char* game_state_to_string(GameState state);

#endif // UNIT_SYSTEM_H