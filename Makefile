CC      ?= gcc
CFLAGS  ?= -O2 -Wall -static -Wextra -std=c11
PREFIX  ?= /usr/local

SRC = src/main.c src/json.c src/ini.c src/pmm.c src/http.c src/repo.c \
      src/install.c src/sha256.c src/sha1.c src/mirrors.c src/pdm.c src/out.c

all: pmm

pmm: $(SRC) $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRC)

install: pmm
	install -m 755 pmm $(PREFIX)/bin/pmm

uninstall:
	rm -f $(PREFIX)/bin/pmm

clean:
	rm -f pmm pmm.exe

.PHONY: all install uninstall clean
