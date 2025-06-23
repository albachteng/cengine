#include <stdio.h>
#include <GL/gl.h>
#include "window.h"

int main(void) {
    if (!window_init()) {
        return -1;
    }
    
    Window* window = window_create(800, 600, "C Engine");
    if (!window) {
        window_terminate();
        return -1;
    }
    
    printf("C Engine initialized successfully\n");
    
    while (!window_should_close(window)) {
        window_poll_events();
        
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        window_swap_buffers(window);
    }
    
    window_destroy(window);
    window_terminate();
    
    return 0;
}