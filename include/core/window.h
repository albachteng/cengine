#ifndef WINDOW_H
#define WINDOW_H

#include <GLFW/glfw3.h>

typedef struct {
    GLFWwindow* handle;
    int width;
    int height;
    const char* title;
    
    // Resize callback support
    void (*resize_callback)(int width, int height, void* user_data);
    void* resize_user_data;
} Window;

int window_init(void);
Window* window_create(int width, int height, const char* title);
void window_destroy(Window* window);
int window_should_close(Window* window);
void window_swap_buffers(Window* window);
void window_poll_events(void);
void window_terminate(void);

// Window resize support
void window_set_resize_callback(Window* window, void (*callback)(int width, int height, void* user_data), void* user_data);
void window_get_size(Window* window, int* width, int* height);

#endif