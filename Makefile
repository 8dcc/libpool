
CC=gcc
CPPFLAGS=
CFLAGS=-ansi -Wall -Wextra -Wpedantic -Wshadow -ggdb3
LDLIBS=

#-------------------------------------------------------------------------------

.PHONY: all benchmark clean

all: libpool-example.out

benchmark: benchmark.out
	./benchmark.sh

clean:
	rm -rf obj/*
	rm -f libpool-example.out benchmark.out

#-------------------------------------------------------------------------------

obj/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

%.out: obj/examples/%.c.o obj/src/libpool.c.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDLIBS)
