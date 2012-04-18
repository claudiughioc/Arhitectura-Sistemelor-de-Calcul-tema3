all:build

build:clean
	gcc -Wall src/main.c -o main

run:build
	./main input/23.ppm

clean:
	rm -f main
