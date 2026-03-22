# INTERFACE.md — diff-match-patch C++ Public API

> For internal algorithm details see **[ARCHITECTURE.md](ARCHITECTURE.md)**.
> For build commands and project setup see **[CLAUDE.md](CLAUDE.md)**.

---

## Quick Start

```cpp
#include "diff_match_patch.h"

diff_match_patch dmp;

// Compare two strings
std::vector<Diff> diffs = dmp.diff_main("Hello world", "Hello C++ world");
dmp.diff_cleanupSemantic(diffs);

// Create and apply a patch
std::vector<Patch> patches = dmp.patch_make("Hello world", "Hello C++ world");
std::string patchText = dmp.patch_toText(patches);
auto [result, applied] = dmp.patch_apply(dmp.patch_fromText(patchText), "Hello world");
```

---

## Data Structures

### `Operation` enum

```cpp
enum Operation { DELETE, INSERT, EQUAL };
```

Represents the type of a single diff operation.

| Value | Meaning |
|-------|---------|
| `DELETE` | Text present in the original (text1) but not in the new (text2) |
| `INSERT` | Text present in the new (text2) but not in the original (text1) |
| `EQUAL`  | Text identical in both |

> Do not reorder or renumber — numeric values are used in internal scoring.

---

### `Diff` struct

```cpp
class Diff {
public:
    Operation   operation;   // DELETE, INSERT, or EQUAL
    std::string text;        // The text for this operation
};
```

Represents one edit operation. Stored by value in `std::vector<Diff>`.

| Method | Description |
|--------|-------------|
| `Diff(Operation, const std::string&)` | Construct with operation and text |
| `Diff()` | Default constructor |
| `bool isNull() const` | True if text is empty/null (unset sentinel) |
| `std::string toString() const` | Human-readable form, e.g. `Diff(INSERT,"foo")` |
| `static std::string strOperation(Operation)` | Converts enum to `"INSERT"/"DELETE"/"EQUAL"` |
| `operator==` / `operator!=` | Compares operation and text |

---

### `Patch` struct

```cpp
class Patch {
public:
    std::vector<Diff> diffs;   // Edit operations within this hunk
    int start1;                // 0-based start offset in text1
    int start2;                // 0-based start offset in text2
    int length1;               // Span in text1 (0 = pure insertion)
    int length2;               // Span in text2 (0 = pure deletion)
};
```

Represents one hunk in a unified diff. Stored by value in `std::vector<Patch>`.

| Method | Description |
|--------|-------------|
| `Patch()` | Zero-initialises all ints; diffs is empty |
| `bool isNull() const` | True if all ints are 0 and diffs is empty |
| `std::string toString() const` | GNU unified diff format: `@@ -L,S +L,S @@\n...` |

---

### `LinesToCharsResult` struct

Internal struct returned by `diff_linesToChars`. Not normally used directly by callers.

```cpp
struct LinesToCharsResult {
    std::u32string           chars1;     // text1 encoded as line indices
    std::u32string           chars2;     // text2 encoded as line indices
    std::vector<std::string> lineArray;  // unique lines, lineArray[0] == ""
};
```

---

## Configuration Parameters

Set directly on the `diff_match_patch` instance before calling methods.

```cpp
diff_match_patch dmp;
dmp.Diff_Timeout      = 2.0f;   // allow up to 2 seconds for diffing
dmp.Match_Threshold   = 0.3f;   // stricter fuzzy match
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `Diff_Timeout` | `float` | `1.0` | Seconds before diff gives up (0 = no limit) |
| `Diff_EditCost` | `short` | `4` | Cost of an empty edit in `diff_cleanupEfficiency` |
| `Match_Threshold` | `float` | `0.5` | Fuzzy match quality gate (0.0 = perfect, 1.0 = anything) |
| `Match_Distance` | `int` | `1000` | Search radius around expected match location |
| `Patch_DeleteThreshold` | `float` | `0.5` | Min match quality for fuzzy patch of large deletions |
| `Patch_Margin` | `short` | `4` | Context lines added around each patch hunk |
| `Match_MaxBits` | `short` | `32` | Max pattern length for Bitap algorithm — do not change |

---

## Diff API

### Core

```cpp
std::vector<Diff> diff_main(const std::string &text1, const std::string &text2);
std::vector<Diff> diff_main(const std::string &text1, const std::string &text2, bool checklines);
```

Compute the difference between `text1` (original) and `text2` (new).
`checklines=true` (default) enables a faster line-level pre-pass for large texts.

---

### Cleanup Passes

Apply after `diff_main` to improve readability or performance. Order matters — always clean up before using diffs for display or further processing.

```cpp
void diff_cleanupSemantic(std::vector<Diff> &diffs);
```
Eliminates trivial equalities, shifting edits to word/sentence boundaries. Best for human-readable output.

```cpp
void diff_cleanupSemanticLossless(std::vector<Diff> &diffs);
```
Slides edits sideways to align with word or line boundaries without changing content.

```cpp
void diff_cleanupEfficiency(std::vector<Diff> &diffs);
```
Collapses short equalities surrounded by edits into DELETE+INSERT pairs. Best for minimising patch size.

```cpp
void diff_cleanupMerge(std::vector<Diff> &diffs);
```
Merges adjacent identical operations and removes redundant equalities. Applied internally; rarely needed by callers.

---

### Utility

```cpp
std::string diff_prettyHtml(const std::vector<Diff> &diffs);
```
Returns an HTML string with `<ins>`/`<del>` spans for browser display.

```cpp
std::string diff_text1(const std::vector<Diff> &diffs);
std::string diff_text2(const std::vector<Diff> &diffs);
```
Reconstruct the original (`text1`) or new (`text2`) string from a diff list.

```cpp
int diff_levenshtein(const std::vector<Diff> &diffs);
```
Returns the Levenshtein edit distance (insertions + deletions + substitutions).

```cpp
int diff_xIndex(const std::vector<Diff> &diffs, int loc);
```
Translate character offset `loc` in `text1` to the equivalent offset in `text2`.

```cpp
int diff_commonPrefix(const std::string &text1, const std::string &text2);
int diff_commonSuffix(const std::string &text1, const std::string &text2);
```
Return the length of the common prefix/suffix of two strings.

---

### Serialization

```cpp
std::string          diff_toDelta(const std::vector<Diff> &diffs);
std::vector<Diff>    diff_fromDelta(const std::string &text1, const std::string &delta);
```

Compact encoding: `=N` keep, `-N` delete, `+text` insert, tab-separated.
`diff_fromDelta` throws `std::string` on malformed input.

---

## Match API

```cpp
int match_main(const std::string &text, const std::string &pattern, int loc);
```

Fuzzy-locate `pattern` within `text`, starting near position `loc`.
- Returns the best matching index, or `-1` if no match within `Match_Threshold`.
- Patterns longer than `Match_MaxBits` (32) characters fall back to exact search.
- Controlled by `Match_Threshold` and `Match_Distance`.

```cpp
// Example: find "world" near position 10, tolerating minor differences
int pos = dmp.match_main("Hello world!", "world", 10);
// pos == 6
```

---

## Patch API

### Create Patches

```cpp
// From two strings (most common)
std::vector<Patch> patch_make(const std::string &text1, const std::string &text2);

// From a diff list (when you already have diffs)
std::vector<Patch> patch_make(const std::string &text1, const std::vector<Diff> &diffs);

// From diffs only (text1 reconstructed internally)
std::vector<Patch> patch_make(const std::vector<Diff> &diffs);

// Deprecated overload — text2 is ignored
std::vector<Patch> patch_make(const std::string &text1, const std::string &text2,
                               const std::vector<Diff> &diffs);
```

### Apply Patches

```cpp
std::pair<std::string, std::vector<bool>>
    patch_apply(std::vector<Patch> &patches, const std::string &text);
```

Apply patches to `text`. Returns:
- `first` — patched text
- `second` — per-patch boolean: `true` = applied successfully, `false` = failed

Internally uses fuzzy matching as a fallback when exact location fails.

### Serialize / Deserialize

```cpp
std::string           patch_toText(const std::vector<Patch> &patches);
std::vector<Patch>    patch_fromText(const std::string &textline);
```

Emit/parse GNU unified diff format (`@@ -L,S +L,S @@`). Throws `std::string` on malformed input.

### Helpers

```cpp
std::vector<Patch>  patch_deepCopy(std::vector<Patch> &patches);
std::string         patch_addPadding(std::vector<Patch> &patches);
void                patch_splitMax(std::vector<Patch> &patches);
```

`patch_deepCopy` — returns a full value copy of the patch list.
`patch_addPadding` / `patch_splitMax` — called internally by `patch_apply`; rarely needed directly.

---

## Error Handling

All errors are thrown as `std::string` (or `const char*` literals). Wrap calls in:

```cpp
try {
    auto diffs = dmp.diff_fromDelta(text1, delta);
} catch (const std::string &e) {
    std::cerr << "Error: " << e << "\n";
} catch (const char *e) {
    std::cerr << "Error: " << e << "\n";
}
```

| Method | Thrown on |
|--------|-----------|
| `diff_main` | Null inputs |
| `diff_fromDelta` | Negative count, unknown op, length mismatch |
| `match_main` | Null inputs |
| `match_bitap` | Pattern longer than `Match_MaxBits` |
| `patch_make` | Null inputs |
| `patch_fromText` | Invalid patch string format |

---

## Typical Usage Patterns

### Show differences between two strings

```cpp
diff_match_patch dmp;
auto diffs = dmp.diff_main(text1, text2);
dmp.diff_cleanupSemantic(diffs);
for (const Diff &d : diffs) {
    if      (d.operation == INSERT) std::cout << "[+]" << d.text;
    else if (d.operation == DELETE) std::cout << "[-]" << d.text;
    else                            std::cout << d.text;
}
```

### Generate and store a patch, apply it later

```cpp
// Generate
auto patches   = dmp.patch_make(original, modified);
std::string stored = dmp.patch_toText(patches);

// Apply later
auto loaded = dmp.patch_fromText(stored);
auto [result, flags] = dmp.patch_apply(loaded, original);
```

### Fuzzy search

```cpp
dmp.Match_Threshold = 0.3f;   // stricter
int pos = dmp.match_main(document, query, expectedPos);
if (pos == -1) { /* not found */ }
```
