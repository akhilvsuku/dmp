CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
SOURCES = diff_match_patch.cpp diff_match_patch_test.cpp
TARGET = diff_match_patch

all: debug

debug: CXXFLAGS += -g
debug: $(TARGET)

release: CXXFLAGS += -O2
release: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)
