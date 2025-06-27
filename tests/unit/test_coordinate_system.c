#include "../minunit.h"
#include "../../include/coordinate_system.h"
#include "../../include/physics.h"
#include "../../include/renderer.h"

int tests_run = 0;

// Test that physics and renderer use consistent coordinate scaling
static char* test_coordinate_system_consistency() {
    // Verify physics boundary matches world boundary
    mu_assert("Physics boundary must match world boundary", 
              PHYSICS_DEFAULT_BOUNDARY_RADIUS == WORLD_BOUNDARY_RADIUS);
    
    // Verify renderer scale factors match world scale
    mu_assert("Renderer X scale must match world scale", 
              RENDER_COORD_SCALE_X == WORLD_COORDINATE_SCALE);
    mu_assert("Renderer Y scale must match world scale", 
              RENDER_COORD_SCALE_Y == WORLD_COORDINATE_SCALE);
    mu_assert("Renderer scale factor must match world scale", 
              RENDER_SCALE_FACTOR == WORLD_COORDINATE_SCALE);
    
    return 0;
}

// Test coordinate conversion functions
static char* test_coordinate_conversions() {
    // Test world to screen conversion
    float world_pos = 50.0f;  // Half the boundary radius
    float screen_pos = world_to_screen_x(world_pos);
    float expected_screen = 50.0f / WORLD_COORDINATE_SCALE;
    
    mu_assert("World to screen X conversion incorrect", screen_pos == expected_screen);
    
    // Test screen to world conversion (round trip)
    float round_trip = screen_to_world_x(screen_pos);
    mu_assert("Screen to world X round trip failed", round_trip == world_pos);
    
    // Test Y coordinates  
    screen_pos = world_to_screen_y(world_pos);
    mu_assert("World to screen Y conversion incorrect", screen_pos == expected_screen);
    
    round_trip = screen_to_world_y(screen_pos);
    mu_assert("Screen to world Y round trip failed", round_trip == world_pos);
    
    return 0;
}

// Test boundary consistency - objects at boundary edge should render at screen edge
static char* test_boundary_rendering_consistency() {
    // Object at positive boundary edge
    float boundary_pos = WORLD_BOUNDARY_RADIUS;
    float screen_pos = world_to_screen_x(boundary_pos);
    
    // Should render at normalized coordinate 1.0 (positive screen edge)
    float expected = WORLD_BOUNDARY_RADIUS / WORLD_COORDINATE_SCALE;
    mu_assert("Boundary object should render at screen edge", screen_pos == expected);
    
    // Object at negative boundary edge
    boundary_pos = -WORLD_BOUNDARY_RADIUS;
    screen_pos = world_to_screen_x(boundary_pos);
    expected = -WORLD_BOUNDARY_RADIUS / WORLD_COORDINATE_SCALE;
    mu_assert("Negative boundary object should render at negative screen edge", 
              screen_pos == expected);
    
    return 0;
}

// Test that coordinate system constants are reasonable
static char* test_coordinate_system_sanity() {
    mu_assert("World boundary radius must be positive", WORLD_BOUNDARY_RADIUS > 0);
    mu_assert("World coordinate scale must be positive", WORLD_COORDINATE_SCALE > 0);
    mu_assert("World boundary should be reasonable size (10-1000)", 
              WORLD_BOUNDARY_RADIUS >= 10.0f && WORLD_BOUNDARY_RADIUS <= 1000.0f);
    
    return 0;
}

static char* all_tests() {
    mu_test_suite_start();
    
    mu_run_test(test_coordinate_system_consistency);
    mu_run_test(test_coordinate_conversions);
    mu_run_test(test_boundary_rendering_consistency);
    mu_run_test(test_coordinate_system_sanity);
    
    return 0;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    char *result = all_tests();
    mu_test_suite_end(result);
    
    return result != 0;
}