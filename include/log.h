#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

// Log levels
typedef enum {
    LOG_TRACE = 0,
    LOG_DEBUG = 1,
    LOG_INFO = 2,
    LOG_WARN = 3,
    LOG_ERROR = 4,
    LOG_FATAL = 5
} LogLevel;

// Log configuration - supports ZII
typedef struct {
    LogLevel min_level;     // Minimum level to log (default: LOG_INFO)
    bool use_colors;        // Enable colored output (default: true)
    bool show_timestamps;   // Show timestamps (default: true)
    FILE* output;          // Output file (default: stdout)
} LogConfig;

// Initialize logging system with ZII config
void log_init(LogConfig* config);

// Core logging functions
void log_write(LogLevel level, const char* file, int line, const char* fmt, ...);

// Convenience macros
#define LOG_TRACE(...) log_write(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) log_write(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_write(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_write(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_write(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) log_write(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

// Performance logging
void log_frame_time(float delta_time);
void log_memory_usage(size_t bytes_used, size_t bytes_total);

// Cleanup
void log_cleanup(void);

#endif