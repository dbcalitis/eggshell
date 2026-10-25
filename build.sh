#!/bin/bash

CC=gcc
CFLAGS=(
    -std=c17
    -Wall
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wformat=2
    -Wstrict-prototypes
)
DEBUG_FLAGS=(
    -g
    -D DEBUG
)
SOURCES=(
    main.c
    tokenizer.c
    memory.c
)
OUT=eggshell

if [ $# -gt 1 ]; then
    echo "[BUILD] eggshell: Too many arguments"
    exit 1
fi

if [ $# -eq 1 ] && [[ "$*" == *--clean* ]]; then
    echo "[BUILD] eggshell: Clean build"
    rm -f eggshell *.so 
elif [ $# -eq 1 ] && [[ "$*" == *--debug* ]]; then
    echo "[BUILD] eggshell: Debug build"
    "$CC" "${DEBUG_FLAGS[@]}" "${CFLAGS[@]}" "${SOURCES[@]}" -o "$OUT"
elif [ $# -eq 1 ] && [[ "$*" == *--unit_tests* ]]; then
    echo "[BUILD] eggshell: Build and running all unit tests"
    python -m pytest -v
else
    echo "[BUILD] eggshell: Regular build"
    "$CC" "${CFLAGS[@]}" "${SOURCES[@]}" -o "$OUT"
fi
