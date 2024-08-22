
CC := gcc
CFLAGS := -lm -g -O3 -std=c2x -MMD
#CFLAGS += -fsanitize=address -lasan

all: build/solve

-include build/solve.d

build/solve: crack-vigenere.c | str.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@ 
