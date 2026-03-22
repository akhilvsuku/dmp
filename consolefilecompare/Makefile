CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

ROOT     = ..
BUILDDIR = $(ROOT)/build/consolefilecompare
TARGET   = $(BUILDDIR)/consolefilecompare

STATIC_LIB = $(ROOT)/libdiff_match_patch.a

.PHONY: all clean lib

# Build the static library first (if not already built), then link the app
all: lib $(BUILDDIR) $(TARGET)

lib:
	$(MAKE) -C $(ROOT) static

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(TARGET): main.cpp $(STATIC_LIB)
	$(CXX) $(CXXFLAGS) -I$(ROOT) -o $@ main.cpp $(STATIC_LIB)

clean:
	rm -f $(TARGET)
