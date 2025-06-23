#ifndef WINDOW_H
#define WINDOW_H

#include <GLFW/glfw3.h>

typedef struct {
    GLFWwindow* handle;
    int width;
    int height;
    const char* title;
} Window;

int window_init(void);
Window* window_create(int width, int height, const char* title);
void window_destroy(Window* window);
int window_should_close(Window* window);
void window_swap_buffers(Window* window);
void window_poll_events(void);
void window_terminate(void);

#endif