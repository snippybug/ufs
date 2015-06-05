

GCC = gcc
FLAGS = -m32

fsformat: fsformat.c fs.h
	$(GCC) $(FLAGS) -o $@ $<

terminal: terminal.c
	$(GCC) $(FLAGS) -o $@ $<
