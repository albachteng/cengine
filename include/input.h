#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <GLFW/glfw3.h>

#define MAX_KEYS 512
#define MAX_MOUSE_BUTTONS 8

typedef struct {
    bool keys[MAX_KEYS];
    bool keys_pressed[MAX_KEYS];
    bool keys_released[MAX_KEYS];
    
    bool mouse_buttons[MAX_MOUSE_BUTTONS];
    bool mouse_buttons_pressed[MAX_MOUSE_BUTTONS];
    bool mouse_buttons_released[MAX_MOUSE_BUTTONS];
    
    double mouse_x, mouse_y;
    double mouse_delta_x, mouse_delta_y;
    double scroll_x, scroll_y;
    
    GLFWwindow* window;
} InputState;

void input_init(InputState* input, GLFWwindow* window);
void input_update(InputState* input);
void input_cleanup(InputState* input);

bool input_key_down(InputState* input, int key);
bool input_key_pressed(InputState* input, int key);
bool input_key_released(InputState* input, int key);

bool input_mouse_down(InputState* input, int button);
bool input_mouse_pressed(InputState* input, int button);
bool input_mouse_released(InputState* input, int button);

void input_get_mouse_position(InputState* input, double* x, double* y);
void input_get_mouse_delta(InputState* input, double* dx, double* dy);
void input_get_scroll(InputState* input, double* scroll_x, double* scroll_y);

extern InputState* g_input;

#endif