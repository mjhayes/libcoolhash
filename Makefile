PREFIX = /usr
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include

CC = gcc
CFLAGS = -Wall -O2 -fPIC
LDFLAGS = -shared

VERSION = $(shell git describe)
VERSION_MAJOR = $(word 1, $(subst ., ,$(VERSION)))
VERSION_MINOR = $(word 2, $(subst ., ,$(VERSION)))
VERSION_RELEASE = $(word 3, $(subst ., ,$(VERSION)))

LIBNAME = libcoolhash
LIB_LINKERNAME = $(LIBNAME).so
LIB_SONAME = $(LIB_LINKERNAME).$(VERSION_MAJOR)
LIB_REALNAME = $(LIB_SONAME).$(VERSION_MINOR).$(VERSION_RELEASE)

OBJS = src/coolhash.o

all: $(LIB_REALNAME)

$(LIB_REALNAME): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

install:
	install -D $(LIB_REALNAME) $(DESTDIR)$(LIBDIR)/$(LIB_REALNAME)
	ln -s $(LIBDIR)/$(LIB_REALNAME) $(DESTDIR)$(LIBDIR)/$(LIB_SONAME)
	ln -s $(LIBDIR)/$(LIB_SONAME) $(DESTDIR)$(LIBDIR)/$(LIB_LINKERNAME)

clean:
	rm -f $(LIB_REALNAME) src/*.o
