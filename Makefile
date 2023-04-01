CC ?= cc
CFLAGS = -Os -std=c99 -g

all: icbm

%.o: %.c
	@printf 'CC	%s\n' $@
	@$(CC) -c -o $@ $(CFLAGS) $^

icbm: main.o log.o irc.o
	@printf 'CC	%s\n' $@
	@$(CC) -o $@ $^

.PHONY: clean

clean:
	rm -f main.o log.o irc.o icbm