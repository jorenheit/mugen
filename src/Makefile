CXX      := g++
CC       := gcc
CXXFLAGS := -Wall --std=c++20
CFLAGS   := -Wall

PREFIX   := /usr/local
BINDIR   := $(PREFIX)/bin

# Targets to build
TARGETS  := mugen

MUGEN_SRCS    := mugen.cc mugen_generate.cc mugen_debug.cc mugen_writer.cc binarywriter.cc cppwriter.cc util.cc
MUGEN_OBJS    := mugen.o  mugen_generate.o mugen_debug.o mugen_writer.o binarywriter.o cppwriter.o util.o linenoise/linenoise.o

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
