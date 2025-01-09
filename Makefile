
CC=gcc
CFLAGS=-ansi -Wall -Wextra -Wpedantic -ggdb3
LDLIBS=

BINS=libpool-test.out benchmark.out

#-------------------------------------------------------------------------------

.PHONY: all benchmark clean

all: libpool-test.out

benchmark: benchmark.out
	./benchmark.sh

clean:
	rm -f obj/*.o
	rm -f libpool-test.out benchmark.out

#-------------------------------------------------------------------------------

$(BINS): %.out: obj/%.c.o obj/libpool.c.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

obj/%.c.o : src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ -c $<
