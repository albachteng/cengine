#include "memory.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

void test_arena_basic() {
    Arena arena = {0};  // ZII
    
    // Test arena initialization
    assert(arena_init(&arena, 1024));
    assert(arena.memory != NULL);
    assert(arena.size == 1024);
    assert(arena.used == 0);
    assert(arena.owns_memory == true);
    
    // Test allocation
    void* ptr1 = arena_alloc(&arena, 64);
    assert(ptr1 != NULL);
    assert(arena.used == 64);
    
    void* ptr2 = arena_alloc(&arena, 32);
    assert(ptr2 != NULL);
    assert(arena.used == 96);  // 64 + 32, aligned to 8
    
    // Test buffer initialization
    char buffer[512] = {0};
    Arena buffer_arena = {0};  // ZII
    arena_init_with_buffer(&buffer_arena, buffer, sizeof(buffer));
    assert(buffer_arena.memory == buffer);
    assert(buffer_arena.size == 512);
    assert(buffer_arena.owns_memory == false);
    
    void* ptr3 = arena_alloc(&buffer_arena, 100);
    assert(ptr3 != NULL);
    assert(ptr3 == buffer);
    
    arena_cleanup(&arena);
    arena_cleanup(&buffer_arena);
    
    printf("Arena basic tests passed!\n");
}

void test_arena_pool() {
    ArenaPool pool = {0};  // ZII
    
    assert(arena_pool_init(&pool));
    assert(pool.arena_count == 1);
    assert(pool.current_arena == 0);
    
    // Test allocation
    void* ptr1 = arena_pool_alloc(&pool, 1000);
    assert(ptr1 != NULL);
    
    // Test stats
    ArenaStats stats = {0};  // ZII
    arena_pool_get_stats(&pool, &stats);
    assert(stats.arena_count == 1);
    assert(stats.used_bytes >= 1000);
    
    arena_pool_cleanup(&pool);
    
    printf("Arena pool tests passed!\n");
}

int main() {
    test_arena_basic();
    test_arena_pool();
    printf("All arena tests passed!\n");
    return 0;
}