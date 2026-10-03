.RECIPEPREFIX := >


SHELL := /bin/term
CC := /bin/gcc
CFLAGS := -O2

all: hello

hello: hello.o
>$(CC) $(CFLAGS) hello.o -o hello

hello.o: hello.c
>$(CC) $(CFLAGS) -c hello.c -o hello.o
