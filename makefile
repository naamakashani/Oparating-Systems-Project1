all: ex3.out

ex3.out: main.c
	gcc -lrt -pthread -o ex3.out main.c


