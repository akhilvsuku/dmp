# CLAUDE.md — diff-match-patch C++

## Project Overview

C++ port of Google's [diff-match-patch](https://github.com/google/diff-match-patch) library. Provides three core algorithms operating on plain text:

- **Diff** — compute the difference between two texts (Myers O(ND) algorithm)
- **Match** — fuzzy-find a pattern within a text (Bitap algorithm)
- **Patch** — generate and apply unified-diff patches with fuzzy fallback

The entry point is the `diff_match_patch` class. `Diff` and `Patch` are plain value types used as data carriers.
License: Apache 2.0.

---

## Tech Stack

| Component | Details |
|-----------|---------|
| Language | C++17 |
| Build system | GNU Make (`Makefile`) |
| String type | `std::string` |
| Containers | `std::vector`, `std::map`, `std::stack` |
| Regex | `std::regex` |
| Time | `std::chrono` / `clock_t` |
| Test framework | Custom hand-rolled harness (no GoogleTest) |
| External dependencies | **None** — C++17 standard library only |

---

## Folder Structure

```
dmp/
├── diff_match_patch.h            Public API — class/method declarations, Diff, Patch value types
├── diff_match_patch.cpp          All algorithm implementations (~2 100 lines)
├── diff_match_patch_test.h       Test harness class declaration (friend of diff_match_patch)
├── diff_match_patch_test.cpp     Test methods covering all three modules
├── Makefile                      Build configuration — test binary, static lib, shared lib
├── consolefilecompare/           Example console app — compare two text files
│   ├── main.cpp
│   └── Makefile
└── build/                        Build artefacts (generated, not version-controlled)
    └── consolefilecompare/
```

There is no `src/`, `include/`, or `test/` split — all source lives at the root level.

---

## Common Commands

### Build

```bash
# Build and run tests (debug)
make

# Build static library: libdiff_match_patch.a
make static

# Build shared library: libdiff_match_patch.so
make dynamic

# Clean
make clean
```

### Run tests

```bash
# The default 'make' target builds and runs tests automatically.
# To run the test binary directly after building:
./diff_match_patch
```

### Build example apps

```bash
# consolefilecompare — compare two text files
cd consolefilecompare
make
# Binary: ../build/consolefilecompare/consolefilecompare

# Usage:
../build/consolefilecompare/consolefilecompare file1.txt file2.txt
```

---

## Coding Conventions

### Naming

| Construct | Convention | Example |
|-----------|------------|---------|
| Classes | PascalCase | `Diff`, `Patch`, `diff_match_patch` |
| Public methods | `module_verb()` snake_case with module prefix | `diff_main()`, `patch_apply()`, `match_bitap()` |
| Config parameters | `Module_Name` (PascalCase with underscore) | `Diff_Timeout`, `Match_Distance` |
| Local variables | camelCase | `bestLoc`, `charCount` |
| Private helpers | same `module_verb()` pattern | `diff_bisectSplit()`, `match_alphabet()` |

### Formatting

- **Indentation:** 4 spaces (no tabs)
- **Encoding:** UTF-8
- **Line endings:** Unix (`\n`)
- Comments follow JavaDoc style for public API; inline comments explain non-obvious algorithm steps

### String and Container Usage

- Always use `std::string` for text that passes through the API
- Prefer STL containers (`std::vector`, `std::map`) throughout
- Use `(void)x;` or leave parameters unnamed for intentionally unused parameters

### Error Handling

- Throw `std::string` exceptions for invalid inputs (e.g. malformed delta, null inputs)
- No error codes; callers must catch `std::string`

### Visibility

- `public` — user-facing API and configuration fields
- `protected` — internal algorithms accessible to subclasses and tests (`diff_bisect`, `patch_addContext`, etc.)
- `private` — implementation details
- `friend class diff_match_patch_test` — allows tests to reach protected/private members without making them public

---

## Architecture Decisions

### Single header + single implementation file

The entire library is `diff_match_patch.h` / `diff_match_patch.cpp`. This is deliberate — the library is self-contained and easy to embed. Do not split into multiple translation units.

### C++ standard library as the only dependency

All string and container needs are served by the C++17 standard library. There is no Boost, no ICU, no third-party dependencies. The library can be embedded in any C++17 project without additional setup.

### Custom test harness

Tests use a hand-rolled `assertEquals`/`assertTrue`/`assertFalse` harness rather than GoogleTest. The test executable is produced by the root Makefile. `main()` lives in `diff_match_patch_test.cpp`.

### Value semantics for Diff and Patch

`Diff` and `Patch` are plain structs with value semantics. They are stored by value in `std::vector<Diff>` and `std::vector<Patch>`. Do not convert them to heap-allocated pointers or shared pointers.

### Myers O(ND) diff with multiple cleanup passes

`diff_main()` runs the Myers bisection algorithm and then applies up to three optional cleanup passes (merge → semantic → efficiency). Cleanup is additive and order-dependent. The pipeline must remain intact.

### Line-mode shortcut

When `checklines=true` (default), `diff_main` first diffs at line granularity (encoding each line as a single character) and then refines only the changed chunks at character granularity. This is a major performance optimisation for large texts.

### Bitap fuzzy matching with configurable threshold

`match_main()` uses the Bitap (shift-or) algorithm. `Match_MaxBits` (default 32) constrains the pattern length the algorithm can handle. Patterns longer than `Match_MaxBits` fall back to exact search.

### Patch serialization uses GNU unified diff format

`patch_toText()` / `patch_fromText()` emit/parse a format compatible with GNU `diff -u`. The `+`, `-`, and `@@ … @@` markers are part of the contract and must not change.

---

## Things That Must NOT Be Changed

1. **Public API signatures** — `diff_main`, `patch_make`, `patch_apply`, `patch_toText`, `patch_fromText`, `match_main`, and all other public methods. Downstream code depends on these exactly.

2. **`Diff` and `Patch` field names and types** — `operation`, `text`, `start1`, `start2`, `length1`, `length2`, `diffs` are part of the serialized patch format.

3. **`Operation` enum values** — `DELETE`, `INSERT`, `EQUAL` must remain in this order; numeric values are used internally in scoring logic.

4. **Patch text format** — the `@@ -L,S +L,S @@` header and `+`/`-`/` ` line prefix convention. Patches serialized by this library may be stored externally and re-applied later.

5. **Delta encoding characters** — `=`, `-`, `+` prefixes in `diff_toDelta` / `diff_fromDelta`.

6. **`Match_MaxBits` default (32)** — changing this silently breaks the Bitap algorithm for patterns near the boundary.

7. **`friend class diff_match_patch_test`** — removing this breaks the test suite's access to protected/private methods.

8. **No third-party dependencies** — the library depends only on the C++17 standard library. Do not introduce external libraries.

9. **`Makefile` source lists** — if files are added/removed, update the Makefile or the build will silently miss files.

10. **The `build/` directory** — generated artefacts. Do not check them in or rely on their contents in code.
