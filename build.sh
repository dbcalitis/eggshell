gcc -o eggshell main.c tokenizer.c memory.c \
    -g -std=c17 \
    -Wall -Wpedantic -Wshadow \
    -Wconversion -Wformat=2 -Wstrict-prototypes \
