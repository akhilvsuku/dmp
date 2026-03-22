# diff-match-patch C++

C++17 port of Google's [diff-match-patch](https://github.com/google/diff-match-patch) library.
Provides three algorithms operating on plain text:

- **Diff** — compute the difference between two texts (Myers O(ND) algorithm)
- **Match** — fuzzy-find a pattern within a text (Bitap algorithm)
- **Patch** — generate and apply unified-diff patches with fuzzy fallback

No external dependencies — C++17 standard library only.
License: [Apache 2.0](https://www.apache.org/licenses/LICENSE-2.0)

---

## Documentation

| Document | Description |
|----------|-------------|
| [INTERFACE.md](INTERFACE.md) | Public API reference — data structures, all methods, usage examples |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Internal design — algorithms, call graphs, data structures, invariants |
| [CLAUDE.md](CLAUDE.md) | Build commands, coding conventions, project setup |

---

## Quick Start

```cpp
#include "diff_match_patch.h"

diff_match_patch dmp;

// Diff two strings
std::vector<Diff> diffs = dmp.diff_main("Hello world", "Hello C++ world");
dmp.diff_cleanupSemantic(diffs);

// Create a patch and apply it
std::vector<Patch> patches = dmp.patch_make("Hello world", "Hello C++ world");
std::string patchText = dmp.patch_toText(patches);
auto [result, applied] = dmp.patch_apply(dmp.patch_fromText(patchText), "Hello world");
// result == "Hello C++ world"
```

See [INTERFACE.md](INTERFACE.md) for the full API reference.

---

## Build

```bash
# Build and run tests
make

# Static library: libdiff_match_patch.a
make static

# Shared library: libdiff_match_patch.so
make dynamic

# Clean
make clean
```

---

## Example Applications

### consolefilecompare

Compares two text files and displays the diff in the terminal with colour-coded output.

```bash
cd consolefilecompare
make
../build/consolefilecompare/consolefilecompare file1.txt file2.txt
```

- Deleted text shown in **red** `[-...-]`
- Inserted text shown in **green** `{+...+}`
- Unchanged text shown in default colour
