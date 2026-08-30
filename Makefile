CC      ?= gcc
CFLAGS  ?= -O2 -Wall -static -Wextra -std=c11
PREFIX  ?= /usr/local

CORE = src/main.c src/json.c src/ini.c src/pmm.c src/http.c src/repo.c \
       src/install.c src/sha256.c src/sha1.c src/mirrors.c src/pdm.c

all: pmm pdm

pmm: $(CORE) $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ $(CORE)

PDM_DEPS = src/pdm_main.c src/pdm.c src/pmm.c src/sha256.c src/install.c \
           src/http.c src/json.c src/ini.c src/mirrors.c src/sha1.c

pdm: $(PDM_DEPS) $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ $(PDM_DEPS)

install: all
	install -m 755 pmm $(PREFIX)/bin/pmm
	install -m 755 pdm $(PREFIX)/bin/pdm

uninstall:
	rm -f $(PREFIX)/bin/pmm $(PREFIX)/bin/pdm

clean:
	rm -f pmm pdm pmm.exe pdm.exe

.PHONY: all install uninstall clean
