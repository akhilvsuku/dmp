# ARCHITECTURE.md — diff-match-patch C++

> **Scope:** Deep internal reference for contributors and engineers modifying the codebase.
> For build commands, project setup, and coding conventions see **[CLAUDE.md](CLAUDE.md)**.
> For public API reference and data structures see **[INTERFACE.md](INTERFACE.md)**.

---

## 1. Overview

This library implements three independent text-processing algorithms in a single C++ class:

- **Diff** — computes the minimal edit script between two strings using the Myers O(ND) bisection algorithm, with an optional line-mode shortcut and multiple cleanup passes.
- **Match** — fuzzy-locates a pattern within a text using the Bitap (shift-or) algorithm, parameterised by error threshold and distance.
- **Patch** — generates and applies GNU unified-diff patches; patch application uses fuzzy matching as a fallback when exact location fails.

All three modules live in a single `diff_match_patch` class. `Diff` and `Patch` are plain value types (no heap allocation, no shared ownership).

---

## 2. File Map

| File | Lines | Purpose |
|------|------:|---------|
| `diff_match_patch.h` | 625 | Public API — `Operation` enum, `Diff`/`Patch` value types, `diff_match_patch` class declaration, `safeMid` inline helpers |
| `diff_match_patch.cpp` | 2106 | All algorithm implementations — Diff, Match, Patch, cleanup passes, serialization |
| `diff_match_patch_test.h` | 90 | Test harness class declaration; `friend` of `diff_match_patch` |
| `diff_match_patch_test.cpp` | 1197 | `main()`, 28 test methods, assert helpers, `diffList()`, `diff_rebuildtexts()` |
| `diff_match_patch.pro` | 20 | qmake build configuration — single `app` target, Debug+Release |
| `diff_match_patch.pro.user` | 278 | Qt Creator per-user IDE settings (generated, not version-controlled) |

---

## 3. Type System

### `Operation` enum (h:66–68)

```cpp
enum Operation { DELETE, INSERT, EQUAL };
```

- `DELETE = 0`, `INSERT = 1`, `EQUAL = 2`
- **Load-bearing values:** the numeric ordering is used in `diff_cleanupSemanticScore` and internal scoring arithmetic. Do not reorder.

---

### `Diff` struct (h:74–94, cpp:39–83)

Plain value type representing a single edit operation.

| Member | Type | Description |
|--------|------|-------------|
| `operation` | `Operation` | One of `DELETE`, `INSERT`, `EQUAL` |
| `text` | `QString` | The text associated with this operation |

**Methods:**

| Method | Description |
|--------|-------------|
| `Diff(Operation, const QString&)` | Initialising constructor |
| `Diff()` | Default constructor (uninitialized fields) |
| `bool isNull() const` | Returns true if text is null (unset) |
| `QString toString() const` | Human-readable `Diff(INSERT,"foo")`, newlines replaced with `¶` |
| `static QString strOperation(Operation)` | Converts enum to `"INSERT"/"DELETE"/"EQUAL"` string |
| `operator==` / `operator!=` | Compares operation and text |

Stored by value in `QList<Diff>`. Never heap-allocated.

---

### `Patch` struct (h:100–114, cpp:96–155)

Plain value type representing one hunk in a unified diff.

| Member | Type | Description |
|--------|------|-------------|
| `diffs` | `QList<Diff>` | Edit operations within this patch |
| `start1` | `int` | 0-based start offset in text1 |
| `start2` | `int` | 0-based start offset in text2 |
| `length1` | `int` | Span in text1 (0 means pure insertion) |
| `length2` | `int` | Span in text2 (0 means pure deletion) |

**Methods:**

| Method | Description |
|--------|-------------|
| `Patch()` | Zero-initialises all four ints; `diffs` is empty |
| `bool isNull() const` | True if all ints are 0 and `diffs` is empty |
| `QString toString()` | Emits GNU unified diff format `@@ -L,S +L,S @@\n...` |

`toString()` notes:
- Indices are printed 1-based (adds 1 to `start1`/`start2`).
- When `length == 1` the `,1` is omitted (GNU convention).
- Each diff line is URL percent-encoded, preserving `" !~*'();/?:@&=+$,#"`.

---

## 4. `diff_match_patch` Class Anatomy

**Friend declaration** (h:123): `friend class diff_match_patch_test` — grants tests access to `protected` and `private` members without exposing them publicly.

**Static members** (cpp:981–982):
```cpp
static QRegExp BLANKLINEEND;   // "\\n\\r?\\n$"
static QRegExp BLANKLINESTART; // "^\\r?\\n\\r?\\n"
```
Used by `diff_cleanupSemanticScore` to detect blank-line boundaries (score 5).

**Static helpers** (h:607–622) — private inline, defined in header:
```cpp
static inline QString safeMid(const QString &str, int pos);
static inline QString safeMid(const QString &str, int pos, int len);
```
Return `""` instead of null when `pos == str.length()`. Used pervasively to avoid null `QString` propagation from `mid()`.

### Configuration Parameters

Set in the constructor (cpp:164–172). All are public fields — callers modify them directly.

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `Diff_Timeout` | `float` | `1.0` | Seconds before `diff_bisect` gives up (0 = infinite). Converted to `clock_t deadline`. |
| `Diff_EditCost` | `short` | `4` | Cost of an empty edit in `diff_cleanupEfficiency`. Equalities shorter than this become split candidates. |
| `Match_Threshold` | `float` | `0.5` | Bitap quality gate: 0.0 = perfect match only, 1.0 = accept anything. |
| `Match_Distance` | `int` | `1000` | Distance from expected location before score is penalised. 0 = must be at exact location. |
| `Patch_DeleteThreshold` | `float` | `0.5` | Minimum Bitap match quality for fuzzy patch application of large deleted blocks. |
| `Patch_Margin` | `short` | `4` | Number of context lines added around each patch hunk. |
| `Match_MaxBits` | `short` | `32` | Maximum pattern length for Bitap; equals `sizeof(int)*8`. Changing this silently overflows the bitmask. |

### Visibility Breakdown

| Visibility | Count | Examples |
|------------|------:|---------|
| Public | 32 | `diff_main`, `patch_apply`, `match_main`, all cleanup methods, `diff_xIndex`, `diff_levenshtein` |
| Protected | 7 | `diff_bisect`, `diff_linesToChars`, `diff_commonOverlap`, `diff_halfMatch`, `match_bitap`, `match_alphabet`, `patch_addContext` |
| Private | 23 | `diff_compute`, `diff_lineMode`, `diff_bisectSplit`, `diff_linesToCharsMunge`, `diff_charsToLines`, `diff_halfMatchI`, `diff_cleanupSemanticScore`, `match_bitapScore`, `safeMid` (×2) |
| Friend | 1 | `diff_match_patch_test` |

---

## 5. Diff Module — Full Method Reference

### 5.1 Entry Points

**`diff_main(text1, text2)`** — public (h:172, cpp:175)
- Calls `diff_main(text1, text2, true)` (enables line-mode shortcut by default).

**`diff_main(text1, text2, checklines)`** — public (h:183, cpp:180)
- Computes `deadline` from `Diff_Timeout` (`clock_t max` if timeout ≤ 0).
- Delegates to `diff_main(text1, text2, checklines, deadline)`.

**`diff_main(text1, text2, checklines, deadline)`** — private (h:198, cpp:192)
- Throws `"Null inputs. (diff_main)"` on null input.
- Shortcut: if `text1 == text2`, returns `[EQUAL, text1]`.
- Strips common prefix and suffix via `diff_commonPrefix`/`diff_commonSuffix`.
- Delegates middle block to `diff_compute`.
- Restores prefix/suffix as EQUAL diffs.
- Calls `diff_cleanupMerge` on the result.

---

### 5.2 Compute Routing

**`diff_compute(text1, text2, checklines, deadline)`** — private (h:212, cpp:237)

Applies speedups before falling through to the full algorithm:

1. **Empty text1** → single INSERT of text2.
2. **Empty text2** → single DELETE of text1.
3. **Substring containment** — if one text contains the other, emits surrounding DELETE/INSERT + inner EQUAL.
4. **Single character shorttext** — emits DELETE+INSERT directly.
5. **Half-match** (`diff_halfMatch`) — if a shared middle substring ≥ half the longer text is found, splits into two recursive `diff_main` calls.
6. **Line mode** — if `checklines && len1 > 100 && len2 > 100`, calls `diff_lineMode`.
7. **Full bisect** — calls `diff_bisect`.

---

### 5.3 Myers Algorithm

**`diff_bisect(text1, text2, deadline)`** — protected (h:235, cpp:369)

Implements Myers 1986 "An O(ND) Difference Algorithm". Simultaneously walks forward and reverse edit graphs until paths overlap, then calls `diff_bisectSplit`.

- Allocates two integer arrays `v1[2*max_d]` and `v2[2*max_d]` on the heap (deleted before return).
- `max_d = (len1 + len2 + 1) / 2` — maximum edit distance to explore.
- `v_offset = max_d` — base index into the arrays (allows negative k values).
- Forward path: `v1[k]` = furthest-right x for diagonal k.
- Reverse path: `v2[k]` = furthest-right x measured from bottom-right corner.
- If `delta % 2 != 0`, forward path may reach reverse path; otherwise reverse may reach forward.
- Detects overlap by comparing `x1 >= x2` (after mirroring `x2` onto top-left coordinates).
- On deadline expiry or no overlap: returns `[DELETE text1, INSERT text2]`.

**`diff_bisectSplit(text1, text2, x, y, deadline)`** — private (h:248, cpp:487)

Splits at the found snake midpoint `(x, y)` and recursively calls `diff_main` (with `checklines=false`) on each half. Results are concatenated.

---

### 5.4 Line-Mode Shortcut

**`diff_lineMode(text1, text2, deadline)`** — private (h:224, cpp:306)

1. Encodes each unique line as a single `QChar` via `diff_linesToChars`.
2. Runs `diff_main` at character (= line) granularity.
3. Decodes back with `diff_charsToLines`.
4. Runs `diff_cleanupSemantic` to remove freak blank-line matches.
5. Re-diffs each contiguous INSERT+DELETE block at character granularity (second pass).

**`diff_linesToChars(text1, text2)`** — protected (h:260, cpp:501)
- Returns `QList<QVariant>{encodedText1, encodedText2, lineArray}`.
- Delegates encoding to `diff_linesToCharsMunge` for each text.
- `lineArray[0]` is always `""` to avoid generating null characters.

**`diff_linesToCharsMunge(text, lineArray, lineHash)`** — private (h:271, cpp:523)
- Walks text line-by-line (no `split()` to avoid memory duplication).
- Assigns each unique line an index into `lineArray`; index stored as `QChar` in output.
- Uses `QMap<QString,int> lineHash` for O(log n) deduplication.

**`diff_charsToLines(diffs, lineArray)`** — private (h:281, cpp:554)
- Replaces each `QChar` in each diff's text with `lineArray[char.unicode()]`.
- Uses `QMutableListIterator<Diff>` for in-place mutation.

---

### 5.5 Common-Substring Helpers

**`diff_commonPrefix(text1, text2)`** — public (h:290, cpp:569)
- Linear scan from front; returns character count.

**`diff_commonSuffix(text1, text2)`** — public (h:299, cpp:582)
- Linear scan from back; returns character count.

**`diff_commonOverlap(text1, text2)`** — protected (h:309, cpp:596)
- Detects whether the suffix of `text1` is a prefix of `text2`.
- Handles truncation and uses binary search for efficiency.
- Returns count of overlapping characters.

**`diff_halfMatch(text1, text2)`** — protected (h:322, cpp)
- Returns a 5-element `QStringList` `[text1_a, text1_b, text2_a, text2_b, mid_common]`, or empty if no half-match.
- Tries two `diff_halfMatchI` calls at quarter-lengths.
- Only applies when `Diff_Timeout > 0` (not worth it for time-unlimited diffs).

**`diff_halfMatchI(longtext, shorttext, i)`** — private (h:335)
- Checks if a substring of `shorttext` starting at `i` appears in `longtext` with ≥ 50% coverage.

---

### 5.6 Cleanup Pipeline

Cleanup passes are additive and order-dependent. `diff_main` applies `diff_cleanupMerge` after every recursive call; callers may additionally invoke the semantic/efficiency passes.

**`diff_cleanupMerge(diffs)`** — public (h:377, cpp)
- Merges adjacent operations of the same type.
- Slides edits as far left as possible.
- Eliminates redundant equalities.
- Repeats until no changes.
- Used internally and by callers wanting a canonical form.

**`diff_cleanupSemantic(diffs)`** — public (h:342, cpp)
- Iterates over equalities, tracking surrounding INSERT/DELETE operations.
- Eliminates equalities shorter than the sum of surrounding edits.
- Calls `diff_cleanupSemanticLossless` to further shift edits to word/sentence boundaries.

**`diff_cleanupSemanticLossless(diffs)`** — public (h:351, cpp)
- For each equality sandwiched between edits, tries sliding the edit left and right.
- Keeps the position with the highest `diff_cleanupSemanticScore`.

**`diff_cleanupSemanticScore(one, two)`** — private (h:362, cpp:945)

Scores a proposed edit boundary `(one | two)`. Higher scores indicate better (more meaningful) boundaries:

| Score | Condition |
|------:|-----------|
| 5 | Blank line at boundary (`BLANKLINEEND` / `BLANKLINESTART`) |
| 4 | Line break character |
| 3 | End of sentence (non-alphanumeric before whitespace) |
| 2 | Whitespace |
| 1 | Non-alphanumeric character |
| 0 | Alphanumeric (worst — split mid-word) |

**`diff_cleanupEfficiency(diffs)`** — public (h:369, cpp:985)
- Converts short equalities (< `Diff_EditCost` chars) surrounded by edits on both sides into DELETE+INSERT pairs, collapsing the equality.
- Uses a `QStack<Diff>` to track candidate equalities.
- Runs `diff_cleanupMerge` after any changes.
- Controlled by `Diff_EditCost` (default 4).

---

### 5.7 Utility / Conversion Methods

**`diff_xIndex(diffs, loc)`** — public (h:388, cpp)
- Translates a character offset in `text1` to the equivalent offset in `text2`.
- Walks diffs accumulating DELETE and EQUAL counts for text1, INSERT and EQUAL for text2.

**`diff_prettyHtml(diffs)`** — public (h:396, cpp)
- Returns an HTML string with `<ins>`/`<del>`/unchanged spans, special-casing `&`, `<`, `>`, `\n`.

**`diff_text1(diffs)`** — public (h:404, cpp)
- Reconstructs original text: concatenates text of all EQUAL and DELETE diffs.

**`diff_text2(diffs)`** — public (h:412, cpp)
- Reconstructs new text: concatenates text of all EQUAL and INSERT diffs.

**`diff_levenshtein(diffs)`** — public (h:421, cpp)
- Counts insertions, deletions, and substitutions (min of adjacent INSERT/DELETE pair lengths).
- Returns the Levenshtein distance.

**`diff_toDelta(diffs)`** — public (h:432, cpp)
- Encodes diff as a compact delta string: `=N` (keep N chars), `-N` (delete N chars), `+text` (insert URL-encoded text), tab-separated.

**`diff_fromDelta(text1, delta)`** — public (h:443, cpp:1373)
- Reconstructs a diff list from `text1` and a delta string.
- Throws on negative counts, unknown operation characters, or length mismatch.

---

## 6. Match Module — Full Method Reference

**`match_main(text, pattern, loc)`** — public (h:458, cpp:1425)
- Throws `"Null inputs. (match_main)"` on null.
- Clamps `loc` to `[0, text.length()]`.
- Returns 0 if `text == pattern` (exact equality shortcut).
- Returns -1 if `text` is empty.
- Returns `loc` if `pattern` matches exactly at `loc` (exact location shortcut).
- Otherwise delegates to `match_bitap`.

**`match_bitap(text, pattern, loc)`** — protected (h:469, cpp:1450)
- Throws `"Pattern too long for this application."` if `pattern.length() > Match_MaxBits`.
- Builds character bitmask via `match_alphabet`.
- Binary searches the scan window per error level `d` (0 … pattern.length()-1).
- Maintains two arrays `rd[]` and `last_rd[]` (heap-allocated per pass, freed after each pass).
- `matchmask = 1 << (pattern.length() - 1)` — detects a full pattern match at each position.
- Early-exit when `match_bitapScore(d+1, loc, loc, pattern) > score_threshold`.
- Returns `best_loc` (-1 if no match within threshold).

**`match_bitapScore(e, x, loc, pattern)`** — private (h:480, cpp:1552)
```
score = (e / pattern.length()) + (|loc - x| / Match_Distance)
```
- `e` = number of errors; `x` = candidate position.
- `Match_Distance == 0` special-cased to avoid divide-by-zero.
- Returns 0.0 (perfect) to >1.0 (too far/too many errors).

**`match_alphabet(pattern)`** — protected (h:488, cpp:1564)
- Returns `QMap<QChar, int>` mapping each character to a bitmask.
- Bit `(pattern.length() - i - 1)` is set for each occurrence of `pattern[i]`.
- Multiple occurrences of the same character OR their bits together.

---

## 7. Patch Module — Full Method Reference

### 7.1 Creation

There are four `patch_make` overloads; they all converge on `patch_make(text1, diffs)` as the core implementation.

| Overload | Signature | Notes |
|----------|-----------|-------|
| 1 | `patch_make(text1, text2)` | Runs `diff_main` then calls overload 4 |
| 2 | `patch_make(diffs)` | Reconstructs `text1` via `diff_text1` then calls overload 4 |
| 3 | `patch_make(text1, text2, diffs)` | `text2` ignored — deprecated; calls overload 4 |
| 4 | `patch_make(text1, diffs)` | **Core.** Throws on null. Iterates diffs to build patches, calls `patch_addContext`. |

**`patch_make(text1, diffs)`** core logic:
- Walks diffs maintaining `char_count1` (position in text1) and `char_count2` (position in text2).
- Opens a new `Patch` when an INSERT or DELETE is encountered outside an open patch.
- Extends EQUAL context up to `Patch_Margin` chars on each side.
- Closes patch and calls `patch_addContext` when accumulated EQUAL exceeds `2 * Patch_Margin`.

**`patch_addContext(patch, text)`** — protected (h:501, cpp:1582)
- Expands the patch context until the pattern substring is unique in `text`.
- Stops when `pattern.length() >= Match_MaxBits - 2*Patch_Margin`.
- Adds one extra `Patch_Margin` for safety.
- Prepends/appends EQUAL diffs and updates `start1`, `start2`, `length1`, `length2`.

---

### 7.2 Application

**`patch_apply(patches, text)`** — public (h:561, cpp)
1. Returns immediately if `patches` is empty.
2. `patch_addPadding` — adds a padding string to start and end of text and adjusts all patch offsets.
3. `patch_splitMax` — splits any patch too large for `Match_MaxBits`.
4. For each patch:
   - Computes expected location from current offset delta.
   - Calls `match_main` to find the actual location.
   - If `match_main` returns -1 (no match), marks patch as failed.
   - Otherwise computes overlap quality; if below `Patch_DeleteThreshold`, re-diffs the matched region and applies that instead.
   - Applies approved diffs directly to the text.
5. Strips padding added in step 2.
6. Returns `QPair<QString, QVector<bool>>` — patched text and per-patch success flags.

**`patch_deepCopy(patches)`** — public (h:550, cpp)
- Returns a deep copy of the patch list (value semantics, but `QList<Diff>` inside each `Patch` is also copied).

**`patch_addPadding(patches)`** — public (h:570, cpp)
- Generates a padding string of `Patch_Margin` non-printable Unicode characters.
- Prepends/appends it to the text passed to `patch_apply`.
- Adjusts `start1`/`start2` of first/last patches accordingly.
- Returns the padding string (used by `patch_apply` to strip it afterward).

**`patch_splitMax(patches)`** — public (h:579, cpp)
- Walks the patch list; for any patch whose `length1 > Match_MaxBits - 2*Patch_Margin - 2`, splits it into smaller patches of at most `Match_MaxBits - 2*Patch_Margin - 2` characters.
- Inserts new patches into the list (using index iteration, not iterators).

---

### 7.3 Serialization

**`patch_toText(patches)`** — public (h:587, cpp)
- Concatenates `patch.toString()` for each patch.
- Each patch emits `@@ -L,S +L,S @@\n` header followed by `+`/`-`/` ` prefixed URL-encoded lines.

**`patch_fromText(textline)`** — public (h:597, cpp:2040)
- Splits on `\n`.
- Expects lines starting with `@@`.
- Throws `"Invalid patch string: ..."` if header format is wrong.
- Parses `@@ -L,S +L,S @@` (handles omitted `,S` when S=1).
- Parses body lines: `+` → INSERT, `-` → DELETE, ` ` → EQUAL (URL-decoded).
- Throws `"Invalid patch mode '...' in: ..."` on unknown prefix.

---

## 8. Internal Data Structures

### Myers Vectors (diff_bisect)
```
v1[0 .. 2*max_d-1]   forward path:  v1[v_offset + k] = furthest x on diagonal k
v2[0 .. 2*max_d-1]   reverse path:  v2[v_offset + k] = furthest x from bottom-right
```
Heap-allocated with `new int[v_length]`, freed before every return path (including early returns via `diff_bisectSplit`).

### Bitap Arrays (match_bitap)
```
rd[0 .. finish+1]      current error pass — bit-vector of partial matches
last_rd[0 .. finish+1] previous error pass
```
Heap-allocated per error level `d`; `last_rd` freed at start of each iteration, `rd` becomes `last_rd`. Both freed at function exit.

### Line Encoding Map (diff_linesToChars / diff_linesToCharsMunge)
```cpp
QStringList lineArray;         // lineArray[i] == the i-th unique line string
QMap<QString, int> lineHash;   // lineHash[line] == index into lineArray
```
`lineArray[0]` is intentionally `""` to keep index 0 unused (null `QChar` avoidance).

### Mutable Iterators (cleanup passes)
`QMutableListIterator<Diff>` is used in `diff_lineMode`, `diff_charsToLines`, `diff_cleanupMerge`, `diff_cleanupSemantic`, `diff_cleanupEfficiency`, and `patch_splitMax` for in-place mutation of `QList<Diff>` without index arithmetic.

---

## 9. Call Graphs

### Diff

```
diff_main(t1, t2)                        [public, no checklines]
  └─ diff_main(t1, t2, checklines=true)  [public]
       └─ diff_main(t1, t2, checklines, deadline)  [private core]
            ├─ diff_commonPrefix
            ├─ diff_commonSuffix
            ├─ diff_compute
            │    ├─ diff_halfMatch
            │    │    └─ diff_halfMatchI
            │    ├─ diff_main  [recursive for each half]
            │    ├─ diff_lineMode
            │    │    ├─ diff_linesToChars
            │    │    │    └─ diff_linesToCharsMunge
            │    │    ├─ diff_main  [recursive, checklines=false]
            │    │    ├─ diff_charsToLines
            │    │    ├─ diff_cleanupSemantic
            │    │    │    └─ diff_cleanupSemanticLossless
            │    │    │         └─ diff_cleanupSemanticScore
            │    │    └─ diff_main  [recursive per replace block]
            │    └─ diff_bisect
            │         └─ diff_bisectSplit
            │              └─ diff_main  [recursive on each split half]
            └─ diff_cleanupMerge
```

### Match

```
match_main(text, pattern, loc)
  └─ match_bitap(text, pattern, loc)
       ├─ match_alphabet(pattern)
       └─ match_bitapScore(e, x, loc, pattern)  [called repeatedly]
```

### Patch

```
patch_apply(patches, text)
  ├─ patch_deepCopy(patches)
  ├─ patch_addPadding(patches)
  ├─ patch_splitMax(patches)
  └─ [for each patch]
       ├─ match_main(text, pattern, expected_loc)
       │    └─ match_bitap → match_alphabet, match_bitapScore
       └─ [fuzzy fallback]
            └─ diff_main(matched_region, patch_text2)
                 └─ [full diff pipeline]
```

---

## 10. Error Handling

All errors are thrown as C-string literals or `QString` values. Callers must `catch(QString)` (and also `catch(const char*)` for the few literal throws).

| # | File:Line | Thrown Value |
|---|-----------|--------------|
| 1 | cpp:57 | `"Invalid operation."` (from `Diff::strOperation`, const char*) |
| 2 | cpp:196 | `"Null inputs. (diff_main)"` (const char*) |
| 3 | cpp:1135 | `"Previous diff should have been an equality."` (const char*) |
| 4 | cpp:1397 | `QString("Negative number in diff_fromDelta: %1")` |
| 5 | cpp:1410 | `QString("Invalid diff operation in diff_fromDelta: %1")` |
| 6 | cpp:1415 | `QString("Delta length (%1) smaller than source text length (%2)")` |
| 7 | cpp:1429 | `"Null inputs. (match_main)"` (const char*) |
| 8 | cpp:1453 | `"Pattern too long for this application."` (const char*) |
| 9 | cpp:1628 | `"Null inputs. (patch_make)"` (const char*) |
| 10 | cpp:2045 | `QString("Invalid patch string: %1")` |
| 11 | cpp:2095 | `QString("Invalid patch mode '%1' in: %2")` |

---

## 11. Test Harness Architecture

**`main()`** (cpp:24) — creates a `diff_match_patch_test` instance, calls `run_all_tests()`, prints "Done."

**`run_all_tests()`** (cpp:38)
- Wraps all 28 test-method calls in a single `try { … } catch(QString)`.
- Uses `QTime t; t.start()` to time the full run.
- Calls tests in this order:
  1. `testDiffCommonPrefix`, `testDiffCommonSuffix`, `testDiffCommonOverlap`, `testDiffHalfmatch`
  2. `testDiffLinesToChars`, `testDiffCharsToLines`
  3. `testDiffCleanupMerge`, `testDiffCleanupSemanticLossless`, `testDiffCleanupSemantic`, `testDiffCleanupEfficiency`
  4. `testDiffPrettyHtml`, `testDiffText`, `testDiffDelta`, `testDiffXIndex`, `testDiffLevenshtein`, `testDiffBisect`, `testDiffMain`
  5. `testMatchAlphabet`, `testMatchBitap`, `testMatchMain`
  6. `testPatchObj`, `testPatchFromText`, `testPatchToText`, `testPatchAddContext`, `testPatchMake`, `testPatchSplitMax`, `testPatchAddPadding`, `testPatchApply`

**Shared state:** a single `diff_match_patch dmp` member (h:62) is used across all tests. Tests that modify configuration parameters (e.g., `Match_Threshold`) must restore them manually.

**Assert helpers** (h:65–75) — 11 overloads, all log `"OK"` on pass via `qDebug`, throw the test case name as a `QString` on failure:

| Overload | Signature |
|----------|-----------|
| `assertEquals` | `(QString, int, int)` |
| `assertEquals` | `(QString, QString, QString)` |
| `assertEquals` | `(QString, Diff, Diff)` |
| `assertEquals` | `(QString, QList<Diff>, QList<Diff>)` |
| `assertEquals` | `(QString, QList<QVariant>, QList<QVariant>)` |
| `assertEquals` | `(QString, QVariant, QVariant)` |
| `assertEquals` | `(QString, QMap<QChar,int>, QMap<QChar,int>)` |
| `assertEquals` | `(QString, QStringList, QStringList)` |
| `assertTrue` | `(QString, bool)` |
| `assertFalse` | `(QString, bool)` |
| `assertEmpty` | `(QString, QStringList)` |

**`diffList(d1..d10)`** (h:80–86)
- Builds a `QList<Diff>` from up to 10 arguments.
- Sentinel: `Diff(INSERT, NULL)` — `isNull()` returns true, signals end of arguments.

**`diff_rebuildtexts(diffs)`** (h:78)
- Reconstructs `[text1, text2]` from a diff list.
- `text1` = EQUAL + DELETE segments; `text2` = EQUAL + INSERT segments.
- Used to verify diff correctness without comparing the diff list directly.

---

## 12. Build System

File: `diff_match_patch.pro`

```qmake
TEMPLATE = app                          # Output is an executable (the test runner)
CONFIG   += qt debug_and_release        # Both Debug and Release targets
mac { CONFIG -= app_bundle }            # No .app bundle on macOS

HEADERS = diff_match_patch.h diff_match_patch_test.h
SOURCES = diff_match_patch.cpp diff_match_patch_test.cpp
```

- No extra Qt modules are declared — Qt Core is implicit.
- `FORMS` and `RESOURCES` are empty (no UI).
- Build artefacts land in `build/Desktop_Qt_6_9_1_MinGW_64_bit-Debug/` (path is IDE-generated).
- **If files are added or removed**, `HEADERS`/`SOURCES` lists must be updated or the build silently misses them.

---

## 13. Key Design Invariants

These constraints must not be violated. Each entry explains the technical reason.

1. **Public API signatures are frozen.**
   `diff_main`, `patch_make`, `patch_apply`, `patch_toText`, `patch_fromText`, `match_main`, and all other public methods form the binary interface. Downstream code depends on name and parameter types exactly.

2. **`Operation` enum ordering: `DELETE=0, INSERT=1, EQUAL=2`.**
   These values are used directly in scoring arithmetic inside `diff_cleanupSemanticScore` and bitwise operations elsewhere. Renumbering silently produces wrong scores.

3. **Patch header format `@@ -L,S +L,S @@`.**
   Patches serialised by this library may be stored externally and reapplied later, possibly by other GNU-diff-compatible tools. The format is a wire contract.

4. **Delta encoding prefixes: `=`, `-`, `+`.**
   `diff_toDelta` and `diff_fromDelta` use these single-character prefixes as the serialization wire format. Changing them breaks any externally stored deltas.

5. **`Match_MaxBits` default is 32.**
   The Bitap algorithm uses a single C++ `int` as a bitmask. `Match_MaxBits` must equal `sizeof(int)*8`. Increasing it silently causes integer overflow in `matchmask = 1 << (pattern.length() - 1)`.

6. **`friend class diff_match_patch_test` must remain.**
   The test suite reaches `protected` and `private` methods (e.g., `diff_bisect`, `match_bitap`, `match_alphabet`, `diff_linesToChars`) directly. Removing the `friend` declaration breaks compilation of the test harness.

7. **Qt is the sole external dependency.**
   All string and container types use Qt Core (`QString`, `QList`, `QMap`, `QStringList`, `QVector`, `QRegExp`, `QUrl`, `QTime`). Introducing non-Qt third-party libraries breaks projects that embed this library without those dependencies.

8. **`diff_match_patch.pro` HEADERS/SOURCES lists must stay in sync.**
   qmake uses these lists to generate the Makefile. A file missing from the list is silently excluded from the build.
