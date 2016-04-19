CFLAGS=-Wall -g

clean:
	rm -f irccd

all:
	make irccd

test:
	./tests/connect.sh

