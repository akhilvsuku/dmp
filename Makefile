CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
AR       = ar
ARFLAGS  = rcs

LIB_SRC  = diff_match_patch.cpp
LIB_OBJ  = diff_match_patch.o
LIB_HDR  = diff_match_patch.h

TEST_SRC = diff_match_patch_test.cpp
TEST_OBJ = diff_match_patch_test.o
TEST_BIN = diff_match_patch

STATIC_LIB = libdiff_match_patch.a
SHARED_LIB = libdiff_match_patch.so

# ── default: build and run tests ────────────────────────────────────────────

all: CXXFLAGS += -g
all: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(LIB_SRC) $(TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $(LIB_SRC) $(TEST_SRC)

# ── static library ──────────────────────────────────────────────────────────

static: CXXFLAGS += -O2
static: $(STATIC_LIB)

$(STATIC_LIB): $(LIB_OBJ)
	$(AR) $(ARFLAGS) $@ $^

# ── dynamic / shared library ────────────────────────────────────────────────

dynamic: CXXFLAGS += -O2 -fPIC
dynamic: $(SHARED_LIB)

$(SHARED_LIB): $(LIB_SRC)
	$(CXX) $(CXXFLAGS) -shared -o $@ $^

# ── pattern rule for object files ───────────────────────────────────────────

$(LIB_OBJ): $(LIB_SRC) $(LIB_HDR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# ── housekeeping ─────────────────────────────────────────────────────────────

clean:
	rm -f $(TEST_BIN) $(LIB_OBJ) $(STATIC_LIB) $(SHARED_LIB)

.PHONY: all static dynamic clean
