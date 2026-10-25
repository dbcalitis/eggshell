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
    Int exit_code = 1;
    U64 remainder = 0;
    U64 addition = 0;

    if (alignment == 0 || result == NULL) { exit_code = 0; }

    if (exit_code == 1)
    {
        remainder = value % alignment;
        addition = (remainder == 0) ? 0 : alignment - remainder;
    }

    if (value > SIZE_MAX - addition) { exit_code = 0; }

    *result = value + addition;

    return exit_code;
}

Int
arena_init(
        Arena *arena,
        U64 requested_capacity)
{
    Int exit_code = 0;
    U64 page_size = 0;
    U64 rounded_capacity = 0;
    void *memory = NULL;

    if (arena == NULL || requested_capacity == 0)
    {
        errno = EINVAL;
        exit_code = -1;
    }

    *arena = Cast(Arena){0};

    {
        S64 signed_page_size = sysconf(_SC_PAGESIZE);

        if (signed_page_size <= 0)
        {
            errno = EINVAL;
            exit_code = -1;
        }
        else
        {
            page_size = Cast(U64)signed_page_size;
        }
    }

    if (exit_code == 0
        && !round_up(requested_capacity,
                    page_size,
                    &rounded_capacity))
    {
        errno = EOVERFLOW;
        exit_code = -1;
    }

    if (exit_code == 0)
    {
        memory = mmap(
                NULL,
                rounded_capacity,
                PROT_NONE,
                MAP_PRIVATE | MAP_ANONYMOUS,
                -1,
                0);
    }

    if (memory == MAP_FAILED)
    { 
        exit_code = -1;
    }

    if (exit_code == 0)
    {
        arena->memory = memory;
        arena->capacity = rounded_capacity;
        arena->offset = 0;
        arena->page_size = page_size;
    }

    return exit_code;
}

void *
arena_alloc(
        Arena *arena,
        U64 request_alloc_size,
        U64 alignment)
{
    U64 allocation_start = 0;
    U64 allocation_end = 0;
    U64 required_writable_bytes = 0;

    if (arena == NULL
        || arena->memory == NULL
        || request_alloc_size == 0
        || alignment == 0
        || (alignment & (alignment - 1)) != 0
        || alignment > arena->page_size)
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

    if (required_writable_bytes > arena->commited)
    {
        U64 length = required_writable_bytes - arena->commited;

        if (mprotect(arena->memory + arena->commited, length, PROT_READ | PROT_WRITE) == -1)
        {
            return NULL;
        }

        arena->commited = required_writable_bytes;
    }

    arena->offset = allocation_end;

    return arena->memory + allocation_start;
}

void
arena_reset(Arena* arena)
{
    if (arena == NULL || arena->memory == NULL)
    {
        return;
    }

    arena->offset = 0;
}

void
arena_destroy(Arena* arena)
{
    if (arena == NULL) { return; }
    
    if (arena->memory != NULL)
    {
        munmap(arena->memory, arena->capacity); 
    }

    *arena = Cast(Arena){0};
}
