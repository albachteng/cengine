#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/gl.h>

static void error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int window_init(void) {
    glfwSetErrorCallback(error_callback);
    
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 0;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    
    return 1;
}

Window* window_create(int width, int height, const char* title) {
    Window* window = malloc(sizeof(Window));
    if (!window) {
        fprintf(stderr, "Failed to allocate memory for window\n");
        return NULL;
    }
    
    // ZII: zero initialize the window structure
    memset(window, 0, sizeof(Window));
    window->width = width;
    window->height = height;
    window->title = title;
    
    window->handle = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!window->handle) {
        fprintf(stderr, "Failed to create GLFW window\n");
        free(window);
        return NULL;
    }
    
    glfwMakeContextCurrent(window->handle);
    glfwSwapInterval(1);
    
    glViewport(0, 0, width, height);
    
    return window;
}

void window_destroy(Window* window) {
    if (window) {
        if (window->handle) {
            glfwDestroyWindow(window->handle);
        }
        free(window);
    }
}

int window_should_close(Window* window) {
    return glfwWindowShouldClose(window->handle);
}

void window_swap_buffers(Window* window) {
    glfwSwapBuffers(window->handle);
}

void window_poll_events(void) {
    glfwPollEvents();
}

void window_terminate(void) {
    glfwTerminate();
}