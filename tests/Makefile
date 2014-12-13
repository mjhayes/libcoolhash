CC = gcc
CFLAGS = -Wall -O2
LIBS = -lcheck $(wildcard ../libcoolhash.so*)
LDFLAGS =

PROGNAME = check_coolhash

OBJS = check_coolhash.o

all: $(PROGNAME)

$(PROGNAME): $(OBJS)
	$(CC) $(OBJS) $(LIBS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install:

test: all
	./$(PROGNAME)

clean:
	rm -f $(PROGNAME) *.o
