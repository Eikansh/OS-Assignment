CC := gcc
CFLAGS := -O3
LDFLAGS := -static
ALL := main

all: $(ALL)

target: main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
clean:
	rm -f $(ALL)
