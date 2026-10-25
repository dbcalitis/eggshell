#!/bin/bash

gcc -o eggshell main.c tokenizer.c memory.c \
    -g -std=c17 -D DEBUG\
    -Wall -Wpedantic -Wshadow \
    -Wconversion -Wformat=2 -Wstrict-prototypes \
