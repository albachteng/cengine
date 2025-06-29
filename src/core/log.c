#include "log.h"
#include <time.h>
#include <string.h>

// ANSI color codes
#define ANSI_RESET   "\033[0m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_WHITE   "\033[37m"
#define ANSI_GRAY    "\033[90m"

// Global log configuration with ZII defaults
static LogConfig g_log_config = {
    .min_level = LOG_INFO,
    .use_colors = true,
    .show_timestamps = true,
    .output = NULL  // Will default to stdout in log_init
};

static const char* log_level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static const char* log_level_colors[] = {
    ANSI_GRAY,    // TRACE
    ANSI_CYAN,    // DEBUG  
    ANSI_GREEN,   // INFO
    ANSI_YELLOW,  // WARN
    ANSI_RED,     // ERROR
    ANSI_MAGENTA  // FATAL
};

void log_init(LogConfig* config) {
    if (config) {
        g_log_config = *config;
    }
    
    // Set default output to stdout if not specified
    if (!g_log_config.output) {
        g_log_config.output = stdout;
    }
}

static void get_timestamp(char* buffer, size_t size) {
    time_t rawtime;
    struct tm* timeinfo;
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    strftime(buffer, size, "%H:%M:%S", timeinfo);
}

static const char* get_filename(const char* path) {
    const char* filename = strrchr(path, '/');
    return filename ? filename + 1 : path;
}

void log_write(LogLevel level, const char* file, int line, const char* fmt, ...) {
    // Filter by minimum level
    if (level < g_log_config.min_level) {
        return;
    }
    
    char timestamp[16];
    if (g_log_config.show_timestamps) {
        get_timestamp(timestamp, sizeof(timestamp));
    }
    
    const char* color_start = "";
    const char* color_end = "";
    
    if (g_log_config.use_colors) {
        color_start = log_level_colors[level];
        color_end = ANSI_RESET;
    }
    
    // Print log header
    if (g_log_config.show_timestamps) {
        fprintf(g_log_config.output, "%s[%s %s %s:%d]%s ", 
                color_start, timestamp, log_level_names[level],
                get_filename(file), line, color_end);
    } else {
        fprintf(g_log_config.output, "%s[%s %s:%d]%s ", 
                color_start, log_level_names[level],
                get_filename(file), line, color_end);
    }
    
    // Print formatted message
    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_config.output, fmt, args);
    va_end(args);
    
    fprintf(g_log_config.output, "\n");
    fflush(g_log_config.output);
}

void log_frame_time(float delta_time) {
    static int frame_count = 0;
    static float total_time = 0.0f;
    
    frame_count++;
    total_time += delta_time;
    
    // Log every 60 frames
    if (frame_count % 60 == 0) {
        float avg_frame_time = total_time / 60.0f;
        float fps = 1.0f / avg_frame_time;
        
        LOG_DEBUG("Frame %d: %.2f ms/frame (%.1f FPS)", 
                  frame_count, avg_frame_time * 1000.0f, fps);
        
        total_time = 0.0f;
    }
}

void log_memory_usage(size_t bytes_used, size_t bytes_total) {
    float usage_percent = (float)bytes_used / bytes_total * 100.0f;
    float mb_used = bytes_used / (1024.0f * 1024.0f);
    float mb_total = bytes_total / (1024.0f * 1024.0f);
    
    LOG_DEBUG("Memory: %.2f/%.2f MB (%.1f%%)", 
              mb_used, mb_total, usage_percent);
}

void log_cleanup(void) {
    if (g_log_config.output && g_log_config.output != stdout && g_log_config.output != stderr) {
        fclose(g_log_config.output);
    }
    
    // Reset to ZII state
    g_log_config = (LogConfig){
        .min_level = LOG_INFO,
        .use_colors = true,
        .show_timestamps = true,
        .output = NULL
    };
}