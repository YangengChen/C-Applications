# Define (atleast) the following targets: all, clean
# all must build the executable file named mapreduce.
# clean must remove all object files and executable files created.
# Look at the introduction doc for information on how make a Makefile.
# This make file must build according to the structure described in the
# introduction doc, if it doesn't you will get a ZERO!
# .exe into bin, .o into build
# $gcc -o myprog myfile1.o myfile2.o
CFLAGS = -Wall -g -Werror -Iinclude

all: clean main.o map_reduce.o
	mkdir bin -p
	gcc $(CFLAGS) -g -o bin/mapreduce build/main.o build/map_reduce.o

main.o: src/main.c
	mkdir build -p
	gcc $(CFLAGS) -c src/main.c -o build/main.o

map_reduce.o: src/map_reduce.c
	mkdir build -p
	gcc $(CFLAGS) -c src/map_reduce.c -o build/map_reduce.o

clean:
	rm -rf bin build

			