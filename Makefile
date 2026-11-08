PREFIX ?= /usr/local
CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c99
LDLIBS  = -lcurl

all: wi

wi: wi.c
	$(CC) $(CFLAGS) -o $@ wi.c $(LDFLAGS) $(LDLIBS)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 wi $(DESTDIR)$(PREFIX)/bin/wi

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/wi

clean:
	rm -f wi

.PHONY: all install uninstall clean
