CFLAGS=-Wall -g -march=x86-64 -pipe

clean:
	rm -rf build/

all:
	make src/irccd
	mkdir build
	mv src/irccd build

test:
	./tests/connect.sh

install:


