CC = gcc
CFLAGS = -g -Wall -Werror -pedantic -Wextra -Iinclude

all: clean utfconverter.o
	mkdir -p bin
	$(CC) -o bin/utf build/utfconverter.o 

utfconverter.o:
	mkdir -p build
	$(CC) $(CFLAGS) -c src/utfconverter.c -o build/utfconverter.o

debug: clean utfconverter.o
	mkdir -p bin
	$(CC) -g -DDEBUG -o bin/utf build/utfconverter.o	

clean:
	rm -rf bin build
