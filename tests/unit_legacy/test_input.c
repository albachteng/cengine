#include "../minunit.h"
#include "core/input.h"

int tests_run = 0;

static char* test_input_initialization() {
    InputState input;
    
    input_init(&input, NULL);
    
    mu_assert("All keys should be false initially", !input.keys[GLFW_KEY_A]);
    mu_assert("All mouse buttons should be false initially", !input.mouse_buttons[GLFW_MOUSE_BUTTON_LEFT]);
    mu_assert("Mouse position should be 0,0 initially", input.mouse_x == 0.0 && input.mouse_y == 0.0);
    mu_assert("Mouse delta should be 0,0 initially", input.mouse_delta_x == 0.0 && input.mouse_delta_y == 0.0);
    
    input_cleanup(&input);
    return 0;
}

static char* test_input_key_functions() {
    InputState input;
    input_init(&input, NULL);
    
    mu_assert("Key should not be down initially", !input_key_down(&input, GLFW_KEY_A));
    mu_assert("Key should not be pressed initially", !input_key_pressed(&input, GLFW_KEY_A));
    mu_assert("Key should not be released initially", !input_key_released(&input, GLFW_KEY_A));
    
    input.keys[GLFW_KEY_A] = true;
    input.keys_pressed[GLFW_KEY_A] = true;
    
    mu_assert("Key should be down after setting", input_key_down(&input, GLFW_KEY_A));
    mu_assert("Key should be pressed after setting", input_key_pressed(&input, GLFW_KEY_A));
    
    input_update(&input);
    
    mu_assert("Key should still be down after update", input_key_down(&input, GLFW_KEY_A));
    mu_assert("Key should not be pressed after update", !input_key_pressed(&input, GLFW_KEY_A));
    
    input_cleanup(&input);
    return 0;
}

static char* test_input_mouse_functions() {
    InputState input;
    input_init(&input, NULL);
    
    mu_assert("Mouse button should not be down initially", !input_mouse_down(&input, GLFW_MOUSE_BUTTON_LEFT));
    mu_assert("Mouse button should not be pressed initially", !input_mouse_pressed(&input, GLFW_MOUSE_BUTTON_LEFT));
    
    input.mouse_buttons[GLFW_MOUSE_BUTTON_LEFT] = true;
    input.mouse_buttons_pressed[GLFW_MOUSE_BUTTON_LEFT] = true;
    
    mu_assert("Mouse button should be down after setting", input_mouse_down(&input, GLFW_MOUSE_BUTTON_LEFT));
    mu_assert("Mouse button should be pressed after setting", input_mouse_pressed(&input, GLFW_MOUSE_BUTTON_LEFT));
    
    input_cleanup(&input);
    return 0;
}

static char* test_input_mouse_position() {
    InputState input;
    input_init(&input, NULL);
    
    input.mouse_x = 100.5;
    input.mouse_y = 200.7;
    
    double x, y;
    input_get_mouse_position(&input, &x, &y);
    
    mu_assert("Mouse X position should match", x == 100.5);
    mu_assert("Mouse Y position should match", y == 200.7);
    
    input_cleanup(&input);
    return 0;
}

static char* test_input_bounds_checking() {
    InputState input;
    input_init(&input, NULL);
    
    mu_assert("Invalid key index should return false", !input_key_down(&input, -1));
    mu_assert("Out of bounds key should return false", !input_key_down(&input, MAX_KEYS));
    mu_assert("Invalid mouse button should return false", !input_mouse_down(&input, -1));
    mu_assert("Out of bounds mouse button should return false", !input_mouse_down(&input, MAX_MOUSE_BUTTONS));
    
    input_cleanup(&input);
    return 0;
}

static char* all_tests() {
    mu_run_test(test_input_initialization);
    mu_run_test(test_input_key_functions);
    mu_run_test(test_input_mouse_functions);
    mu_run_test(test_input_mouse_position);
    mu_run_test(test_input_bounds_checking);
    return 0;
}

int main() {
    mu_test_suite_start();
    char* result = all_tests();
    mu_test_suite_end(result);
}