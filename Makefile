.PHONY: main build run dev

main: build run

build:
	g++ -std=c++23 -O2 main.cpp -o main.out

run:
	./main.out

dev:
	g++ -std=c++23 -Wall -Wextra -g main.cpp -o main.out && ./main.out