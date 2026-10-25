#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"

typedef struct arena Arena;
struct arena
{
    U8 *memory;
    U64 capacity;
    U64 offset;         // next free byte
    U64 commited; // bytes currently PROT_READ | PROT_WRITE
    U64 page_size;
};

typedef struct arena_mark Arena_Mark;
struct arena_mark
{
    U64 offset;
};

Int arena_init(Arena *arena, U64 requested_capacity);
void * arena_alloc(Arena *arena, U64 request_alloc_size, U64 alignment);
void arena_reset(Arena* arena);
void arena_destroy(Arena* arena);

#endif // MEMORY_H
