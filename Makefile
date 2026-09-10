main:
	g++ -std=c++23 -Wall -Wextra -g main.cpp -o main.out && ./main.out
build:
	g++ -std=c++23 main.cpp -o main.out
run:
	./main.out
