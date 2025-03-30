CXX      := g++
CXXFLAGS := -Wall --std=c++23
TARGET   := mugen
SRC      := main.cc util.cc mugen.cc
OBJ      := $(SRC:.cc=.o)
PREFIX   := /usr/local
BINDIR   := $(PREFIX)/bin

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

install: $(TARGET)
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean install
