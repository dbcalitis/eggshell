
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "common.h"
#include "memory.h"

static int
round_up(
        U64 value,
        U64 alignment,
        U64 *result)
{
    U64 remainder = 0;
    U64 amount_to_add = 0;

    if (alignment == 0 || result == NULL) { return 0; }

    remainder = value % alignment;

    if (remainder == 0) { amount_to_add = 0; }
    else { amount_to_add = alignment - remainder; }

    if (value > SIZE_MAX - amount_to_add) { return 0; }

    *result = value + amount_to_add;
    return 1;
}

Int
arena_init(
        Arena *arena,
        U64 requested_capacity)
{
    S64 system_page_size = 0;
    U64 rounded_capacity;
    void *memory;

    if (arena == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    memset(arena, 0, sizeof *arena);

    system_page_size = sysconf(_SC_PAGESIZE);
    if (system_page_size <= 0)
    {
        errno = EINVAL;
        return -1;
    }

    arena->page_size = Cast(U64)system_page_size;

    void *allocated = mmap(NULL, requested_capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, 0);

    if (!round_up(requested_capacity, arena->page_size, &rounded_capacity))
    {
        errno = EINVAL;
        return -1;
    }

    memory = mmap(NULL, rounded_capacity, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (memory == MAP_FAILED) { return -1; }

    arena->memory_start = allocated;
    arena->capacity = rounded_capacity;
    arena->offset = 0;

    return 0;
}

void *
arena_alloc(
        Arena *arena,
        U64 request_alloc_size)
{
    U64 allocation_start = 0;
    U64 allocation_end = 0;
    U64 required_writable_bytes = 0;
    U64 alignment = 0;

    if (arena == NULL
        || arena->memory_start == NULL
        || request_alloc_size == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    if (!round_up(arena->offset, alignment, &allocation_start))
    {
        errno = EOVERFLOW;
        return NULL;
    }

    if (allocation_start > arena->capacity
        || request_alloc_size > arena->capacity - allocation_start)
    {
        errno = ENOMEM;
        return NULL;
    }

    allocation_end = allocation_start + request_alloc_size;

    if (!round_up(allocation_end, arena->page_size, &required_writable_bytes))
    {
        errno = EOVERFLOW;
        return NULL;
    }

    if (required_writable_bytes > arena->writable_bytes)
    {
        U64 length = required_writable_bytes - arena->writable_bytes;

        if (mprotect(arena->memory_start + arena->writable_bytes, length, PROT_READ | PROT_WRITE) == -1)
        {
            return NULL;
        }

        arena->writable_bytes = required_writable_bytes;
    }

    arena->offset = allocation_end;

    return arena->memory_start + allocation_start;
}

void
arena_destroy(
        Arena* arena)
{
    if (arena == NULL) { return; }
    
    if (arena->memory_start != NULL) { munmap(arena->memory_start, arena->capacity); }

    memset(arena, 0, sizeof *arena);
}
