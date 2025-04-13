CXX      := g++
CC       := gcc
CXXFLAGS := -Wall --std=c++20
CFLAGS   := -Wall

PREFIX   := /usr/local
BINDIR   := $(PREFIX)/bin

# Targets to build
TARGETS  := mugen

MUGEN_SRCS    := main.cc util.cc mugen.cc
MUGEN_OBJS    := main.o util.o mugen.o linenoise/linenoise.o

.PHONY: all install clean

all: $(TARGETS)

mugen: $(MUGEN_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

mudb: $(MUDB_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Pattern rule for C++ source files (.cc)
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Explicit rule to compile the C file from linenoise directory.
linenoise/linenoise.o: linenoise/linenoise.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: all
	install -d $(BINDIR)
	install -m 755 mugen $(BINDIR)/

clean:
	rm -f *.o linenoise/*.o $(TARGETS)
