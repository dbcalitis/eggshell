gcc -o eggshell main.c memory.c \
    -std=c17 \
    -Wall -Wpedantic -Wshadow \
    -Wconversion -Wformat=2 -Wstrict-prototypes \
