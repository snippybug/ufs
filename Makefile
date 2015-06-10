

GCC = gcc
FLAGS = -m32

ifeq ($(DEBUG),1)
FLAGS += -g
endif


all: terminal

fsformat: fsformat.c 
	$(GCC) $(FLAGS) -o $@ $^

terminal: terminal.c file.c
	$(GCC) $(FLAGS) -o $@ $^
