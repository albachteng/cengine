#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Arena allocator constants
#define ARENA_DEFAULT_SIZE (1024 * 1024)  // 1MB default arena
#define ARENA_ALIGNMENT 8                  // 8-byte alignment for most platforms
#define ARENA_MAX_ARENAS 16               // Maximum number of arenas per pool

// Memory arena structure - supports ZII
typedef struct {
    char* memory;           // Pointer to allocated memory block
    size_t size;           // Total size of the arena
    size_t used;           // Current offset/used bytes
    bool owns_memory;      // Whether this arena owns its memory block
} Arena;

// Arena pool for managing multiple arenas - supports ZII
typedef struct {
    Arena arenas[ARENA_MAX_ARENAS];
    size_t arena_count;
    size_t current_arena;  // Index of current arena for allocation
} ArenaPool;

// ZII-compatible arena initialization (use static memory block)
void arena_init_with_buffer(Arena* arena, void* buffer, size_t size);

// Traditional arena initialization (allocates memory)
bool arena_init(Arena* arena, size_t size);

// Cleanup arena (only frees if owns_memory is true)
void arena_cleanup(Arena* arena);

// Reset arena to beginning (keeps memory allocated)
void arena_reset(Arena* arena);

// Allocate from arena with alignment
void* arena_alloc(Arena* arena, size_t size);
void* arena_alloc_aligned(Arena* arena, size_t size, size_t alignment);

// Arena pool functions - ZII compatible
bool arena_pool_init(ArenaPool* pool);
void arena_pool_cleanup(ArenaPool* pool);
void arena_pool_reset(ArenaPool* pool);

// Allocate from pool (creates new arena if current is full)
void* arena_pool_alloc(ArenaPool* pool, size_t size);

// Get current memory usage statistics
typedef struct {
    size_t total_size;
    size_t used_bytes;
    size_t free_bytes;
    size_t arena_count;
} ArenaStats;

void arena_get_stats(const Arena* arena, ArenaStats* stats);
void arena_pool_get_stats(const ArenaPool* pool, ArenaStats* stats);

// Temporary arena scope helpers (for scoped allocations)
typedef struct {
    Arena* arena;
    size_t saved_used;
} ArenaScope;

ArenaScope arena_scope_begin(Arena* arena);
void arena_scope_end(ArenaScope scope);

#endif