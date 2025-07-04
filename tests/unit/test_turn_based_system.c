#include "../../tests/minunit.h"
#include "game/unit_system.h"
#include "game/map_system.h"
#include "core/ecs.h"
#include <stdio.h>

// Required by minunit
int tests_run = 0;

// get_best_enemy_moves is now exposed in unit_system.h

// Test data
static ECS test_ecs;
static Map test_map;
static TurnManager test_manager;
static ComponentType transform_type;
static ComponentType unit_type;

// Setup function for each test
void setup_test_environment() {
    // Initialize ECS
    ecs_init(&test_ecs);
    
    // Register component types
    transform_type = ecs_register_component(&test_ecs, sizeof(Transform));
    unit_system_init(&test_ecs, &unit_type);
    
    // Initialize a small test map
    map_init(&test_map, MAP_GRID, 5, 5, 20.0f);
    
    // Set up basic terrain (mostly plains with some water)
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            MapCoord coord = grid_coord(x, y);
            TerrainType terrain = TERRAIN_PLAINS;
            
            // Add water barrier in the middle
            if (x == 2 && y >= 1 && y <= 3) {
                terrain = TERRAIN_WATER;
            }
            
            map_set_terrain(&test_map, coord, terrain);
        }
    }
    
    // Initialize turn manager
    turn_manager_init(&test_manager);
}

void cleanup_test_environment() {
    map_cleanup(&test_map);
    ecs_cleanup(&test_ecs);
}

// Test basic unit creation
static char* test_unit_creation() {
    setup_test_environment();
    
    // Create a player unit
    MapCoord player_pos = grid_coord(1, 1);
    Entity player = unit_create(&test_ecs, transform_type, unit_type, UNIT_PLAYER, player_pos, 100);
    
    mu_assert("Player entity should be valid", player != 0);
    
    // Check unit component
    Unit* unit_comp = (Unit*)ecs_get_component(&test_ecs, player, unit_type);
    mu_assert("Unit component should exist", unit_comp != NULL);
    mu_assert("Unit should be alive", unit_comp->is_alive);
    mu_assert("Unit should have correct health", unit_comp->current_health == 100);
    mu_assert("Unit should have correct max health", unit_comp->max_health == 100);
    mu_assert("Unit should be player type", unit_comp->type == UNIT_PLAYER);
    
    // Check transform component
    Transform* transform_comp = (Transform*)ecs_get_component(&test_ecs, player, transform_type);
    mu_assert("Transform component should exist", transform_comp != NULL);
    
    cleanup_test_environment();
    return NULL;
}

// Test turn manager initialization
static char* test_turn_manager_init() {
    setup_test_environment();
    
    mu_assert("Turn manager should start in player turn", test_manager.current_state == GAME_STATE_PLAYER_TURN);
    mu_assert("Turn manager should not be waiting for delay", !test_manager.waiting_for_delay);
    mu_assert("Turn manager should have correct attack damage", test_manager.attack_damage == 5);
    
    cleanup_test_environment();
    return NULL;
}

// Test movement validation
static char* test_movement_validation() {
    setup_test_environment();
    
    // Test valid position (plains)
    MapCoord valid_pos = grid_coord(1, 1);
    mu_assert("Plains should be valid for movement", can_move_to_position(&test_map, valid_pos));
    
    // Test invalid position (water)
    MapCoord water_pos = grid_coord(2, 2);
    mu_assert("Water should be invalid for movement", !can_move_to_position(&test_map, water_pos));
    
    // Test out of bounds
    MapCoord oob_pos = grid_coord(-1, -1);
    mu_assert("Out of bounds should be invalid", !can_move_to_position(&test_map, oob_pos));
    
    cleanup_test_environment();
    return NULL;
}

// Test unit attack mechanics
static char* test_unit_attack() {
    setup_test_environment();
    
    // Create two units
    Entity player = unit_create(&test_ecs, transform_type, unit_type, UNIT_PLAYER, grid_coord(0, 0), 100);
    Entity enemy = unit_create(&test_ecs, transform_type, unit_type, UNIT_ENEMY, grid_coord(0, 0), 30);
    
    Unit* player_unit = (Unit*)ecs_get_component(&test_ecs, player, unit_type);
    Unit* enemy_unit = (Unit*)ecs_get_component(&test_ecs, enemy, unit_type);
    
    // Test attack
    perform_attack(player_unit, enemy_unit, 10);
    
    mu_assert("Enemy should take damage", enemy_unit->current_health == 20);
    mu_assert("Enemy should show damage flash", enemy_unit->show_damage_flash);
    mu_assert("Player should not take damage", player_unit->current_health == 100);
    
    // Test lethal attack
    perform_attack(player_unit, enemy_unit, 25);
    
    mu_assert("Enemy should be dead", !enemy_unit->is_alive);
    mu_assert("Enemy should have 0 health", enemy_unit->current_health == 0);
    
    cleanup_test_environment();
    return NULL;
}

// Test enemy AI pathfinding around obstacles
static char* test_enemy_ai_pathfinding() {
    setup_test_environment();
    
    // Create player and enemy on opposite sides of water barrier
    MapCoord player_pos = grid_coord(1, 2);  // Left side of water
    MapCoord enemy_pos = grid_coord(3, 2);   // Right side of water
    
    Entity player = unit_create(&test_ecs, transform_type, unit_type, UNIT_PLAYER, player_pos, 100);
    Entity enemy = unit_create(&test_ecs, transform_type, unit_type, UNIT_ENEMY, enemy_pos, 30);
    
    // Set positions on map
    Transform* player_transform = (Transform*)ecs_get_component(&test_ecs, player, transform_type);
    Transform* enemy_transform = (Transform*)ecs_get_component(&test_ecs, enemy, transform_type);
    
    player_transform->position = map_coord_to_world(&test_map, player_pos);
    enemy_transform->position = map_coord_to_world(&test_map, enemy_pos);
    
    map_set_occupant(&test_map, player_pos, player);
    map_set_occupant(&test_map, enemy_pos, enemy);
    
    test_manager.player_entity = player;
    test_manager.enemy_entity = enemy;
    
    // Test that enemy can find valid moves (should go around water)
    MapCoord best_moves[4];
    int num_moves = get_best_enemy_moves(&test_ecs, &test_map, transform_type, unit_type, 
                                        enemy_pos, player_pos, best_moves, 4);
    
    mu_assert("Enemy should find valid moves", num_moves > 0);
    
    // All moves should be valid terrain
    for (int i = 0; i < num_moves; i++) {
        mu_assert("AI moves should be on valid terrain", can_move_to_position(&test_map, best_moves[i]));
    }
    
    cleanup_test_environment();
    return NULL;
}

// Test turn state transitions
static char* test_turn_transitions() {
    setup_test_environment();
    
    // Start in player turn
    mu_assert("Should start in player turn", test_manager.current_state == GAME_STATE_PLAYER_TURN);
    
    // End player turn
    turn_manager_end_player_turn(&test_manager);
    
    mu_assert("Should be waiting for delay", test_manager.waiting_for_delay);
    mu_assert("Pending state should be enemy turn", test_manager.pending_state == GAME_STATE_ENEMY_TURN);
    
    // Simulate delay completion
    test_manager.waiting_for_delay = false;
    test_manager.current_state = test_manager.pending_state;
    
    mu_assert("Should now be enemy turn", test_manager.current_state == GAME_STATE_ENEMY_TURN);
    
    cleanup_test_environment();
    return NULL;
}

// Test damage visual effects
static char* test_damage_visual_effects() {
    setup_test_environment();
    
    Entity unit_entity = unit_create(&test_ecs, transform_type, unit_type, UNIT_PLAYER, grid_coord(0, 0), 100);
    Unit* unit = (Unit*)ecs_get_component(&test_ecs, unit_entity, unit_type);
    
    // Initially no damage flash
    mu_assert("Unit should not show damage flash initially", !unit->show_damage_flash);
    mu_assert("Damage flash timer should be 0", unit->damage_flash_timer == 0.0f);
    
    // Take damage
    unit_take_damage(unit, 10);
    
    mu_assert("Unit should show damage flash after taking damage", unit->show_damage_flash);
    mu_assert("Damage flash timer should be positive", unit->damage_flash_timer > 0.0f);
    
    // Update visual effects (simulate time passing)
    unit_update_visual_effects(unit, 1.0f); // 1 second should clear the flash
    
    mu_assert("Damage flash should be cleared after timeout", !unit->show_damage_flash);
    mu_assert("Damage flash timer should be 0", unit->damage_flash_timer == 0.0f);
    
    cleanup_test_environment();
    return NULL;
}

// Main test runner
static char* all_tests() {
    mu_run_test(test_unit_creation);
    mu_run_test(test_turn_manager_init);
    mu_run_test(test_movement_validation);
    mu_run_test(test_unit_attack);
    mu_run_test(test_enemy_ai_pathfinding);
    mu_run_test(test_turn_transitions);
    mu_run_test(test_damage_visual_effects);
    return NULL;
}

int main(void) {
    printf("Running turn-based system tests...\n");
    
    char* result = all_tests();
    
    if (result != NULL) {
        printf("FAILED: %s\n", result);
        return 1;
    } else {
        printf("ALL TESTS PASSED\n");
        printf("Tests run: %d\n", tests_run);
        return 0;
    }
}