#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Helper function to align a size to the given alignment
static size_t align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

void arena_init_with_buffer(Arena* arena, void* buffer, size_t size) {
    // ZII: arena should already be zero-initialized
    arena->memory = (char*)buffer;
    arena->size = size;
    arena->used = 0;
    arena->owns_memory = false;  // External buffer, don't free
}

bool arena_init(Arena* arena, size_t size) {
    // ZII: arena should already be zero-initialized
    if (size == 0) {
        size = ARENA_DEFAULT_SIZE;
    }
    
    arena->memory = (char*)malloc(size);
    if (!arena->memory) {
        return false;
    }
    
    arena->size = size;
    arena->used = 0;
    arena->owns_memory = true;  // We allocated it, we free it
    return true;
}

void arena_cleanup(Arena* arena) {
    if (arena->owns_memory && arena->memory) {
        free(arena->memory);
    }
    // Reset to ZII state
    memset(arena, 0, sizeof(Arena));
}

void arena_reset(Arena* arena) {
    arena->used = 0;
    // Keep memory allocated and ownership status
}

bool arena_expand_if_needed(Arena* arena, size_t upcoming_alloc_size) {
    if (!arena || !arena->memory || !arena->owns_memory) {
        return false;  // Can't expand external buffers
    }
    
    // Calculate usage after the upcoming allocation
    size_t aligned_used = align_size(arena->used, ARENA_ALIGNMENT);
    size_t projected_usage = aligned_used + upcoming_alloc_size;
    
    // Check if we'll exceed the threshold
    float usage_ratio = (float)projected_usage / (float)arena->size;
    if (usage_ratio <= ARENA_EXPANSION_THRESHOLD) {
        return true;  // No expansion needed
    }
    
    // Double the arena size
    size_t new_size = arena->size * 2;
    char* new_memory = (char*)realloc(arena->memory, new_size);
    if (!new_memory) {
        return false;  // Expansion failed
    }
    
    arena->memory = new_memory;
    arena->size = new_size;
    return true;
}

void* arena_alloc(Arena* arena, size_t size) {
    return arena_alloc_aligned(arena, size, ARENA_ALIGNMENT);
}

void* arena_alloc_aligned(Arena* arena, size_t size, size_t alignment) {
    if (!arena || !arena->memory || size == 0) {
        return NULL;
    }
    
    // Try to expand arena if needed before allocation
    if (!arena_expand_if_needed(arena, size)) {
        // Expansion failed, but maybe we still have enough space
    }
    
    // Align the current position
    size_t aligned_used = align_size(arena->used, alignment);
    
    // Check if we have enough space
    if (aligned_used + size > arena->size) {
        return NULL;  // Arena full and couldn't expand
    }
    
    void* ptr = arena->memory + aligned_used;
    arena->used = aligned_used + size;
    
    return ptr;
}

bool arena_pool_init(ArenaPool* pool) {
    // ZII: pool should already be zero-initialized
    // Create first arena
    if (!arena_init(&pool->arenas[0], ARENA_DEFAULT_SIZE)) {
        return false;
    }
    
    pool->arena_count = 1;
    pool->current_arena = 0;
    return true;
}

void arena_pool_cleanup(ArenaPool* pool) {
    for (size_t i = 0; i < pool->arena_count; i++) {
        arena_cleanup(&pool->arenas[i]);
    }
    // Reset to ZII state
    memset(pool, 0, sizeof(ArenaPool));
}

void arena_pool_reset(ArenaPool* pool) {
    for (size_t i = 0; i < pool->arena_count; i++) {
        arena_reset(&pool->arenas[i]);
    }
    pool->current_arena = 0;
}

void* arena_pool_alloc(ArenaPool* pool, size_t size) {
    if (!pool || pool->arena_count == 0) {
        return NULL;
    }
    
    // Try current arena first
    void* ptr = arena_alloc(&pool->arenas[pool->current_arena], size);
    if (ptr) {
        return ptr;
    }
    
    // Current arena is full, try to create a new one
    if (pool->arena_count < ARENA_MAX_ARENAS) {
        size_t new_arena_size = ARENA_DEFAULT_SIZE;
        // If allocation is larger than default, make arena big enough
        if (size > new_arena_size) {
            new_arena_size = align_size(size * 2, ARENA_DEFAULT_SIZE);
        }
        
        if (arena_init(&pool->arenas[pool->arena_count], new_arena_size)) {
            pool->current_arena = pool->arena_count;
            pool->arena_count++;
            return arena_alloc(&pool->arenas[pool->current_arena], size);
        }
    }
    
    // Try other existing arenas as fallback
    for (size_t i = 0; i < pool->arena_count; i++) {
        if (i != pool->current_arena) {
            ptr = arena_alloc(&pool->arenas[i], size);
            if (ptr) {
                pool->current_arena = i;
                return ptr;
            }
        }
    }
    
    return NULL;  // All arenas full and can't create new one
}

void arena_get_stats(const Arena* arena, ArenaStats* stats) {
    // ZII: stats should already be zero-initialized
    if (!arena || !stats) {
        return;
    }
    
    stats->total_size = arena->size;
    stats->used_bytes = arena->used;
    stats->free_bytes = arena->size - arena->used;
    stats->arena_count = 1;
}

void arena_pool_get_stats(const ArenaPool* pool, ArenaStats* stats) {
    // ZII: stats should already be zero-initialized
    if (!pool || !stats) {
        return;
    }
    
    for (size_t i = 0; i < pool->arena_count; i++) {
        stats->total_size += pool->arenas[i].size;
        stats->used_bytes += pool->arenas[i].used;
    }
    stats->free_bytes = stats->total_size - stats->used_bytes;
    stats->arena_count = pool->arena_count;
}

ArenaScope arena_scope_begin(Arena* arena) {
    ArenaScope scope = {0};  // ZII
    scope.arena = arena;
    scope.saved_used = arena ? arena->used : 0;
    return scope;
}

void arena_scope_end(ArenaScope scope) {
    if (scope.arena) {
        scope.arena->used = scope.saved_used;
    }
}