# CLAUDE.md — diff-match-patch C++

## Project Overview

C++ port of Google's [diff-match-patch](https://github.com/google/diff-match-patch) library. Provides three core algorithms operating on plain text:

- **Diff** — compute the difference between two texts (Myers O(ND) algorithm)
- **Match** — fuzzy-find a pattern within a text (Bitap algorithm)
- **Patch** — generate and apply unified-diff patches with fuzzy fallback

The entry point is the `diff_match_patch` class. `Diff` and `Patch` are plain value types used as data carriers.
License: Apache 2.0. Originally ported to Qt/C++ by Mike Slemmer.

---

## Tech Stack

| Component | Details |
|-----------|---------|
| Language | C++17 |
| Build system | qmake (`.pro` file) |
| UI toolkit / core lib | **Qt Core only** (no GUI) — Qt 4.3 through 6.9.x |
| Compiler (tested) | MinGW GCC 13.1.0 (x86_64), cross-platform via Qt |
| String type | `QString` (UTF-8 / Unicode) |
| Containers | `QList`, `QMap`, `QStringList`, `QVector`, `QPair`, `QVariant` |
| Regex | `QRegExp` |
| Time | `QTime` |
| URL encoding | `QUrl::toPercentEncoding()` |
| Test framework | Custom hand-rolled harness (no GoogleTest / QtTest) |
| External dependencies | **None** beyond Qt Core |

---

## Folder Structure

```
cpp/
├── diff_match_patch.h            Public API — class/method declarations, Diff, Patch value types
├── diff_match_patch.cpp          All algorithm implementations (~2 100 lines)
├── diff_match_patch_test.h       Test harness class declaration (friend of diff_match_patch)
├── diff_match_patch_test.cpp     27 test methods covering all three modules (~1 200 lines)
├── diff_match_patch.pro          qmake project file — the only build configuration
├── diff_match_patch.pro.user     Qt Creator per-user IDE settings (generated, not version-controlled)
└── build/                        qmake / Qt Creator build artefacts (generated, not version-controlled)
    └── Desktop_Qt_6_9_1_MinGW_64_bit-Debug/
```

There is no `src/`, `include/`, or `test/` split — all source lives at the root level. Do not reorganise this structure without updating the `.pro` file.

---

## Common Commands

### Build

```bash
# Generate Makefile from qmake project
qmake diff_match_patch.pro

# Compile (debug + release targets defined in .pro)
make

# Release build
make release

# Clean
make clean
```

### Run tests

```bash
# After building, the output binary IS the test runner (main() is in diff_match_patch_test.cpp)
./diff_match_patch
# On Windows (MinGW):
./release/diff_match_patch.exe
```

Test output is written via `qDebug()`. A passing run prints "OK" for each assertion; failures print the failing test name and values.

### Qt Creator

Open `diff_match_patch.pro` in Qt Creator. Build/run/debug via IDE as usual.

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

- **Indentation:** 4 spaces (no tabs) — enforced in Qt Creator project settings
- **Encoding:** UTF-8
- **Line endings:** Unix (`\n`)
- Comments follow JavaDoc style for public API; inline comments explain non-obvious algorithm steps

### Qt Usage

- Always use `QString` — never `std::string` for text that passes through the API
- Prefer Qt containers (`QList`, `QMap`) over STL equivalents for consistency with Qt types
- Use `Q_UNUSED(x)` for intentionally unused parameters
- Use `foreach` (Qt macro) rather than range-for where existing code uses it

### Error Handling

- Throw `QString` exceptions for invalid inputs (e.g. malformed delta, null inputs)
- No error codes; callers must catch `QString`

### Visibility

- `public` — user-facing API and configuration fields
- `protected` — internal algorithms accessible to subclasses and tests (`diff_bisect`, `patch_addContext`, etc.)
- `private` — implementation details
- `friend class diff_match_patch_test` — allows tests to reach protected/private members without making them public

---

## Architecture Decisions

### Single header + single implementation file

The entire library is `diff_match_patch.h` / `diff_match_patch.cpp`. This is deliberate — the original Google port keeps the library self-contained and easy to embed. Do not split into multiple translation units.

### Qt as the only dependency

All string and container needs are served by Qt Core. There is no Boost, no ICU, no STL strings at the API boundary. This keeps the library usable in any Qt project without additional setup.

### Custom test harness

Tests use a hand-rolled `assertEquals`/`assertTrue`/`assertFalse` harness rather than QtTest or GoogleTest. The test executable is produced by the same `.pro` file as the library (there is no separate test project). `main()` lives in `diff_match_patch_test.cpp`.

### Value semantics for Diff and Patch

`Diff` and `Patch` are plain structs with value semantics. They are stored by value in `QList<Diff>` and `QList<Patch>`. Do not convert them to heap-allocated pointers or shared pointers.

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

8. **Qt as the sole dependency** — do not introduce non-Qt third-party libraries. The library is intentionally dependency-free beyond Qt.

9. **`diff_match_patch.pro` `HEADERS`/`SOURCES` lists** — if files are added/removed, update these lists or the build will silently miss files.

10. **The `build/` and `.pro.user` directories** — these are generated artefacts. Do not check them in or rely on their contents in code.
