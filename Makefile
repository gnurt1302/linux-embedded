all:
	gcc -o main main.c -pthread
clean:
	rm -rf main
