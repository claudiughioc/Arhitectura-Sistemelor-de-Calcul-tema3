all:build

build:clean
	gcc -Wall src/main.c -o main

run:build
	./main input/23.ppm output/23_out.ppm 2 4 8 10 10

clean:
	rm -f main
