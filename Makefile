

GCC = gcc
FLAGS = -m32

all: terminal

fsformat: fsformat.c 
	$(GCC) $(FLAGS) -o $@ $^

terminal: terminal.c file.c
	$(GCC) $(FLAGS) -o $@ $^
