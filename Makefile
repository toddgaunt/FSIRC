CFLAGS=-Wall -g

clean:
	rm -rf build/

all:
	make src/irccd
	mkdir build
	mv src/irccd build

test:
	./tests/connect.sh

install:


