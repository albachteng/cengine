#include "input.h"
#include <string.h>
#include <stdio.h>

InputState* g_input = NULL;

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)scancode;
    (void)mods;
    
    if (!g_input || key < 0 || key >= MAX_KEYS) return;
    
    if (action == GLFW_PRESS) {
        g_input->keys[key] = true;
        g_input->keys_pressed[key] = true;
    } else if (action == GLFW_RELEASE) {
        g_input->keys[key] = false;
        g_input->keys_released[key] = true;
    }
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)window;
    (void)mods;
    
    if (!g_input || button < 0 || button >= MAX_MOUSE_BUTTONS) return;
    
    if (action == GLFW_PRESS) {
        g_input->mouse_buttons[button] = true;
        g_input->mouse_buttons_pressed[button] = true;
    } else if (action == GLFW_RELEASE) {
        g_input->mouse_buttons[button] = false;
        g_input->mouse_buttons_released[button] = true;
    }
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    
    if (!g_input) return;
    
    g_input->mouse_delta_x = xpos - g_input->mouse_x;
    g_input->mouse_delta_y = ypos - g_input->mouse_y;
    g_input->mouse_x = xpos;
    g_input->mouse_y = ypos;
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    
    if (!g_input) return;
    
    g_input->scroll_x = xoffset;
    g_input->scroll_y = yoffset;
}

void input_init(InputState* input, GLFWwindow* window) {
    // ZII: zero initialize input state
    memset(input, 0, sizeof(InputState));
    input->window = window;
    g_input = input;
    
    if (window) {
        glfwSetKeyCallback(window, key_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetCursorPosCallback(window, cursor_position_callback);
        glfwSetScrollCallback(window, scroll_callback);
        
        glfwGetCursorPos(window, &input->mouse_x, &input->mouse_y);
    }
}

void input_update(InputState* input) {
    memset(input->keys_pressed, 0, sizeof(input->keys_pressed));
    memset(input->keys_released, 0, sizeof(input->keys_released));
    memset(input->mouse_buttons_pressed, 0, sizeof(input->mouse_buttons_pressed));
    memset(input->mouse_buttons_released, 0, sizeof(input->mouse_buttons_released));
    
    input->mouse_delta_x = 0.0;
    input->mouse_delta_y = 0.0;
    input->scroll_x = 0.0;
    input->scroll_y = 0.0;
}

void input_cleanup(InputState* input) {
    if (input && input->window) {
        glfwSetKeyCallback(input->window, NULL);
        glfwSetMouseButtonCallback(input->window, NULL);
        glfwSetCursorPosCallback(input->window, NULL);
        glfwSetScrollCallback(input->window, NULL);
    }
    g_input = NULL;
}

bool input_key_down(InputState* input, int key) {
    if (!input || key < 0 || key >= MAX_KEYS) return false;
    return input->keys[key];
}

bool input_key_pressed(InputState* input, int key) {
    if (!input || key < 0 || key >= MAX_KEYS) return false;
    return input->keys_pressed[key];
}

bool input_key_released(InputState* input, int key) {
    if (!input || key < 0 || key >= MAX_KEYS) return false;
    return input->keys_released[key];
}

bool input_mouse_down(InputState* input, int button) {
    if (!input || button < 0 || button >= MAX_MOUSE_BUTTONS) return false;
    return input->mouse_buttons[button];
}

bool input_mouse_pressed(InputState* input, int button) {
    if (!input || button < 0 || button >= MAX_MOUSE_BUTTONS) return false;
    return input->mouse_buttons_pressed[button];
}

bool input_mouse_released(InputState* input, int button) {
    if (!input || button < 0 || button >= MAX_MOUSE_BUTTONS) return false;
    return input->mouse_buttons_released[button];
}

void input_get_mouse_position(InputState* input, double* x, double* y) {
    if (!input) return;
    if (x) *x = input->mouse_x;
    if (y) *y = input->mouse_y;
}

void input_get_mouse_delta(InputState* input, double* dx, double* dy) {
    if (!input) return;
    if (dx) *dx = input->mouse_delta_x;
    if (dy) *dy = input->mouse_delta_y;
}

void input_get_scroll(InputState* input, double* scroll_x, double* scroll_y) {
    if (!input) return;
    if (scroll_x) *scroll_x = input->scroll_x;
    if (scroll_y) *scroll_y = input->scroll_y;
}