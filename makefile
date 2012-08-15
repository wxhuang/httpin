SRC = httpin.c hget.c
CC = gcc

all:$(SRC)
	$(CC) $^ -o test

