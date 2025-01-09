
CC=gcc
CFLAGS=-ansi -Wall -Wextra -Wpedantic -ggdb3
LDLIBS=

SRCS=libpool-test.c libpool.c
OBJS=$(addprefix obj/, $(addsuffix .o, $(SRCS)))

BIN=libpool-test.out

#-------------------------------------------------------------------------------

.PHONY: all clean

all: $(BIN)

clean:
	rm -f $(OBJS)
	rm -f $(BIN)

#-------------------------------------------------------------------------------

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

obj/%.c.o : src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ -c $<
