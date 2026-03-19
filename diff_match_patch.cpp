/*
 * Diff Match and Patch
 * Copyright 2018 The diff-match-patch Authors.
 * https://github.com/google/diff-match-patch
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <sstream>
#include <iomanip>
#include "diff_match_patch.h"


//////////////////////////
//
// String Utility Helpers (free functions in anonymous namespace)
//
//////////////////////////

namespace {

std::string percentEncodeImpl(const std::string &text, const std::string &safe) {
    std::string result;
    result.reserve(text.size() * 3);
    for (unsigned char c : text) {
        if ((std::isalnum(c) && c < 128)
            || c == '-' || c == '_' || c == '.' || c == '~'
            || safe.find((char)c) != std::string::npos) {
            result += (char)c;
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", (unsigned int)c);
            result += buf;
        }
    }
    return result;
}

std::string percentDecodeImpl(const std::string &text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) {
            char hex[3] = { text[i+1], text[i+2], '\0' };
            char *end;
            long val = std::strtol(hex, &end, 16);
            if (end == hex + 2) {
                result += (char)(unsigned char)val;
                i += 2;
                continue;
            }
        }
        result += text[i];
    }
    return result;
}

std::string replaceAllImpl(std::string str, const std::string &from, const std::string &to) {
    if (from.empty()) return str;
    std::size_t start = 0;
    while ((start = str.find(from, start)) != std::string::npos) {
        str.replace(start, from.length(), to);
        start += to.length();
    }
    return str;
}

std::vector<std::string> splitStringImpl(const std::string &str, char delim, bool skipEmpty) {
    std::vector<std::string> tokens;
    std::string token;
    for (char c : str) {
        if (c == delim) {
            if (!skipEmpty || !token.empty()) {
                tokens.push_back(token);
            }
            token.clear();
        } else {
            token += c;
        }
    }
    if (!skipEmpty || !token.empty()) {
        tokens.push_back(token);
    }
    return tokens;
}

bool strStartsWithImpl(const std::string &str, const std::string &prefix) {
    if (prefix.size() > str.size()) return false;
    return str.substr(0, prefix.size()) == prefix;
}

bool strEndsWithImpl(const std::string &str, const std::string &suffix) {
    if (suffix.size() > str.size()) return false;
    return str.substr(str.size() - suffix.size()) == suffix;
}

} // anonymous namespace


// diff_match_patch static wrappers (delegate to anonymous namespace implementations)
std::string diff_match_patch::percentEncode(const std::string &text, const std::string &safe) {
    return percentEncodeImpl(text, safe);
}

std::string diff_match_patch::percentDecode(const std::string &text) {
    return percentDecodeImpl(text);
}

std::string diff_match_patch::replaceAll(std::string str, const std::string &from, const std::string &to) {
    return replaceAllImpl(str, from, to);
}

std::vector<std::string> diff_match_patch::splitString(const std::string &str, char delim, bool skipEmpty) {
    return splitStringImpl(str, delim, skipEmpty);
}

bool diff_match_patch::strStartsWith(const std::string &str, const std::string &prefix) {
    return strStartsWithImpl(str, prefix);
}

bool diff_match_patch::strEndsWith(const std::string &str, const std::string &suffix) {
    return strEndsWithImpl(str, suffix);
}


//////////////////////////
//
// Diff Class
//
//////////////////////////


/**
 * Constructor.  Initializes the diff with the provided values.
 * @param operation One of INSERT, DELETE or EQUAL
 * @param text The text being applied
 */
Diff::Diff(Operation _operation, const std::string &_text) :
  operation(_operation), text(_text) {
  // Construct a diff with the specified operation and text.
}

Diff::Diff() {
}


std::string Diff::strOperation(Operation op) {
  switch (op) {
    case INSERT:
      return "INSERT";
    case DELETE:
      return "DELETE";
    case EQUAL:
      return "EQUAL";
  }
  throw "Invalid operation.";
}

/**
 * Display a human-readable version of this Diff.
 * @return text version
 */
std::string Diff::toString() const {
  std::string prettyText = text;
  // Replace linebreaks with Pilcrow signs (UTF-8: 0xC2 0xB6).
  prettyText = replaceAllImpl(prettyText, "\n", "\xc2\xb6");
  return std::string("Diff(") + strOperation(operation) + std::string(",\"")
      + prettyText + std::string("\")");
}

/**
 * Is this Diff equivalent to another Diff?
 */
bool Diff::operator==(const Diff &d) const {
  return (d.operation == this->operation) && (d.text == this->text);
}

bool Diff::operator!=(const Diff &d) const {
  return !(operator == (d));
}

inline bool Diff::isNull() const {
  return text.empty() && operation == INSERT;
}


/////////////////////////////////////////////
//
// Patch Class
//
/////////////////////////////////////////////


/**
 * Constructor.  Initializes with an empty list of diffs.
 */
Patch::Patch() :
  start1(0), start2(0),
  length1(0), length2(0) {
}

bool Patch::isNull() const {
  if (start1 == 0 && start2 == 0 && length1 == 0 && length2 == 0
      && diffs.size() == 0) {
    return true;
  }
  return false;
}


/**
 * Emulate GNU diff's format.
 * Header: @@ -382,8 +481,9 @@
 * Indices are printed as 1-based, not 0-based.
 * @return The GNU diff string
 */
std::string Patch::toString() const {
  std::string coords1, coords2;
  if (length1 == 0) {
    coords1 = std::to_string(start1) + std::string(",0");
  } else if (length1 == 1) {
    coords1 = std::to_string(start1 + 1);
  } else {
    coords1 = std::to_string(start1 + 1) + std::string(",")
        + std::to_string(length1);
  }
  if (length2 == 0) {
    coords2 = std::to_string(start2) + std::string(",0");
  } else if (length2 == 1) {
    coords2 = std::to_string(start2 + 1);
  } else {
    coords2 = std::to_string(start2 + 1) + std::string(",")
        + std::to_string(length2);
  }
  std::string text;
  text = std::string("@@ -") + coords1 + std::string(" +") + coords2
      + std::string(" @@\n");
  // Escape the body of the patch with %xx notation.
  for (const Diff &aDiff : diffs) {
    switch (aDiff.operation) {
      case INSERT:
        text += std::string("+");
        break;
      case DELETE:
        text += std::string("-");
        break;
      case EQUAL:
        text += std::string(" ");
        break;
    }
    text += percentEncodeImpl(aDiff.text, " !~*'();/?:@&=+$,#")
        + std::string("\n");
  }

  return text;
}


/////////////////////////////////////////////
//
// diff_match_patch Class
//
/////////////////////////////////////////////

// Define static regex members
std::regex diff_match_patch::BLANKLINEEND("\\n\\r?\\n$");
std::regex diff_match_patch::BLANKLINESTART("^\\r?\\n\\r?\\n");

diff_match_patch::diff_match_patch() :
  Diff_Timeout(1.0f),
  Diff_EditCost(4),
  Match_Threshold(0.5f),
  Match_Distance(1000),
  Patch_DeleteThreshold(0.5f),
  Patch_Margin(4),
  Match_MaxBits(32) {
}


std::vector<Diff> diff_match_patch::diff_main(const std::string &text1,
                                              const std::string &text2) {
  return diff_main(text1, text2, true);
}

std::vector<Diff> diff_match_patch::diff_main(const std::string &text1,
    const std::string &text2, bool checklines) {
  // Set a deadline by which time the diff must be complete.
  clock_t deadline;
  if (Diff_Timeout <= 0) {
    deadline = std::numeric_limits<clock_t>::max();
  } else {
    deadline = clock() + (clock_t)(Diff_Timeout * CLOCKS_PER_SEC);
  }
  return diff_main(text1, text2, checklines, deadline);
}

std::vector<Diff> diff_match_patch::diff_main(const std::string &text1,
    const std::string &text2, bool checklines, clock_t deadline) {
  // Check for equality (speedup).
  std::vector<Diff> diffs;
  if (text1 == text2) {
    if (!text1.empty()) {
      diffs.push_back(Diff(EQUAL, text1));
    }
    return diffs;
  }

  // Trim off common prefix (speedup).
  int commonlength = diff_commonPrefix(text1, text2);
  const std::string commonprefix = text1.substr(0, commonlength);
  std::string textChopped1 = text1.substr(commonlength);
  std::string textChopped2 = text2.substr(commonlength);

  // Trim off common suffix (speedup).
  commonlength = diff_commonSuffix(textChopped1, textChopped2);
  const std::string commonsuffix = textChopped1.substr(textChopped1.size() - commonlength);
  textChopped1 = textChopped1.substr(0, textChopped1.size() - commonlength);
  textChopped2 = textChopped2.substr(0, textChopped2.size() - commonlength);

  // Compute the diff on the middle block.
  diffs = diff_compute(textChopped1, textChopped2, checklines, deadline);

  // Restore the prefix and suffix.
  if (!commonprefix.empty()) {
    diffs.insert(diffs.begin(), Diff(EQUAL, commonprefix));
  }
  if (!commonsuffix.empty()) {
    diffs.push_back(Diff(EQUAL, commonsuffix));
  }

  diff_cleanupMerge(diffs);

  return diffs;
}


std::vector<Diff> diff_match_patch::diff_compute(std::string text1, std::string text2,
    bool checklines, clock_t deadline) {
  std::vector<Diff> diffs;

  if (text1.empty()) {
    // Just add some text (speedup).
    diffs.push_back(Diff(INSERT, text2));
    return diffs;
  }

  if (text2.empty()) {
    // Just delete some text (speedup).
    diffs.push_back(Diff(DELETE, text1));
    return diffs;
  }

  {
    const std::string longtext = text1.size() > text2.size() ? text1 : text2;
    const std::string shorttext = text1.size() > text2.size() ? text2 : text1;
    const std::size_t i = longtext.find(shorttext);
    if (i != std::string::npos) {
      // Shorter text is inside the longer text (speedup).
      const Operation op = (text1.size() > text2.size()) ? DELETE : INSERT;
      diffs.push_back(Diff(op, longtext.substr(0, i)));
      diffs.push_back(Diff(EQUAL, shorttext));
      diffs.push_back(Diff(op, safeMid(longtext, (int)(i + shorttext.size()))));
      return diffs;
    }

    if (shorttext.size() == 1) {
      // Single character string.
      // After the previous speedup, the character can't be an equality.
      diffs.push_back(Diff(DELETE, text1));
      diffs.push_back(Diff(INSERT, text2));
      return diffs;
    }
    // Garbage collect longtext and shorttext by scoping out.
  }

  // Check to see if the problem can be split in two.
  const std::vector<std::string> hm = diff_halfMatch(text1, text2);
  if (hm.size() > 0) {
    // A half-match was found, sort out the return data.
    const std::string text1_a = hm[0];
    const std::string text1_b = hm[1];
    const std::string text2_a = hm[2];
    const std::string text2_b = hm[3];
    const std::string mid_common = hm[4];
    // Send both pairs off for separate processing.
    const std::vector<Diff> diffs_a = diff_main(text1_a, text2_a,
                                                 checklines, deadline);
    const std::vector<Diff> diffs_b = diff_main(text1_b, text2_b,
                                                 checklines, deadline);
    // Merge the results.
    diffs = diffs_a;
    diffs.push_back(Diff(EQUAL, mid_common));
    diffs.insert(diffs.end(), diffs_b.begin(), diffs_b.end());
    return diffs;
  }

  // Perform a real diff.
  if (checklines && text1.size() > 100 && text2.size() > 100) {
    return diff_lineMode(text1, text2, deadline);
  }

  return diff_bisect(text1, text2, deadline);
}


std::vector<Diff> diff_match_patch::diff_lineMode(std::string text1, std::string text2,
    clock_t deadline) {
  // Scan the text on a line-by-line basis first.
  LinesToCharsResult b = diff_linesToChars(text1, text2);
  const std::u32string chars1 = b.chars1;
  const std::u32string chars2 = b.chars2;
  std::vector<std::string> linearray = b.lineArray;

  // Diff the encoded strings (u32string).
  std::vector<Diff> diffs = diff_main_u32(chars1, chars2, false, deadline);

  // Convert the diff back to original text.
  diff_charsToLines_u32(diffs, linearray);
  // Eliminate freak matches (e.g. blank lines)
  diff_cleanupSemantic(diffs);

  // Rediff any replacement blocks, this time character-by-character.
  // Add a dummy entry at the end.
  diffs.push_back(Diff(EQUAL, ""));
  int count_delete = 0;
  int count_insert = 0;
  std::string text_delete;
  std::string text_insert;

  int pointer = 0;
  while (pointer < (int)diffs.size()) {
    switch (diffs[pointer].operation) {
      case INSERT:
        count_insert++;
        text_insert += diffs[pointer].text;
        pointer++;
        break;
      case DELETE:
        count_delete++;
        text_delete += diffs[pointer].text;
        pointer++;
        break;
      case EQUAL:
        // Upon reaching an equality, check for prior redundancies.
        if (count_delete >= 1 && count_insert >= 1) {
          // Delete the offending records and add the merged ones.
          std::vector<Diff> newDiffs = diff_main(text_delete, text_insert, false, deadline);
          int start = pointer - count_delete - count_insert;
          diffs.erase(diffs.begin() + start, diffs.begin() + pointer);
          pointer = start;
          for (const Diff &newDiff : newDiffs) {
            diffs.insert(diffs.begin() + pointer, newDiff);
            pointer++;
          }
        }
        count_insert = 0;
        count_delete = 0;
        text_delete = "";
        text_insert = "";
        pointer++;
        break;
    }
  }
  diffs.pop_back();  // Remove the dummy entry at the end.

  return diffs;
}


std::vector<Diff> diff_match_patch::diff_bisect(const std::string &text1,
    const std::string &text2, clock_t deadline) {
  // Cache the text lengths to prevent multiple calls.
  const int text1_length = (int)text1.size();
  const int text2_length = (int)text2.size();
  const int max_d = (text1_length + text2_length + 1) / 2;
  const int v_offset = max_d;
  const int v_length = 2 * max_d;
  // +2 ensures v1[v_offset+1] is in bounds even when max_d == 1.
  int *v1 = new int[v_length + 2];
  int *v2 = new int[v_length + 2];
  for (int x = 0; x < v_length; x++) {
    v1[x] = -1;
    v2[x] = -1;
  }
  v1[v_offset + 1] = 0;
  v2[v_offset + 1] = 0;
  const int delta = text1_length - text2_length;
  // If the total number of characters is odd, then the front path will
  // collide with the reverse path.
  const bool front = (delta % 2 != 0);
  // Offsets for start and end of k loop.
  // Prevents mapping of space beyond the grid.
  int k1start = 0;
  int k1end = 0;
  int k2start = 0;
  int k2end = 0;
  for (int d = 0; d < max_d; d++) {
    // Bail out if deadline is reached.
    if (clock() > deadline) {
      break;
    }

    // Walk the front path one step.
    for (int k1 = -d + k1start; k1 <= d - k1end; k1 += 2) {
      const int k1_offset = v_offset + k1;
      int x1;
      if (k1 == -d || (k1 != d && v1[k1_offset - 1] < v1[k1_offset + 1])) {
        x1 = v1[k1_offset + 1];
      } else {
        x1 = v1[k1_offset - 1] + 1;
      }
      int y1 = x1 - k1;
      while (x1 < text1_length && y1 < text2_length
          && text1[x1] == text2[y1]) {
        x1++;
        y1++;
      }
      v1[k1_offset] = x1;
      if (x1 > text1_length) {
        // Ran off the right of the graph.
        k1end += 2;
      } else if (y1 > text2_length) {
        // Ran off the bottom of the graph.
        k1start += 2;
      } else if (front) {
        int k2_offset = v_offset + delta - k1;
        if (k2_offset >= 0 && k2_offset < v_length && v2[k2_offset] != -1) {
          // Mirror x2 onto top-left coordinate system.
          int x2 = text1_length - v2[k2_offset];
          if (x1 >= x2) {
            // Overlap detected.
            delete [] v1;
            delete [] v2;
            return diff_bisectSplit(text1, text2, x1, y1, deadline);
          }
        }
      }
    }

    // Walk the reverse path one step.
    for (int k2 = -d + k2start; k2 <= d - k2end; k2 += 2) {
      const int k2_offset = v_offset + k2;
      int x2;
      if (k2 == -d || (k2 != d && v2[k2_offset - 1] < v2[k2_offset + 1])) {
        x2 = v2[k2_offset + 1];
      } else {
        x2 = v2[k2_offset - 1] + 1;
      }
      int y2 = x2 - k2;
      while (x2 < text1_length && y2 < text2_length
          && text1[text1_length - x2 - 1] == text2[text2_length - y2 - 1]) {
        x2++;
        y2++;
      }
      v2[k2_offset] = x2;
      if (x2 > text1_length) {
        // Ran off the left of the graph.
        k2end += 2;
      } else if (y2 > text2_length) {
        // Ran off the top of the graph.
        k2start += 2;
      } else if (!front) {
        int k1_offset = v_offset + delta - k2;
        if (k1_offset >= 0 && k1_offset < v_length && v1[k1_offset] != -1) {
          int x1 = v1[k1_offset];
          int y1 = v_offset + x1 - k1_offset;
          // Mirror x2 onto top-left coordinate system.
          x2 = text1_length - x2;
          if (x1 >= x2) {
            // Overlap detected.
            delete [] v1;
            delete [] v2;
            return diff_bisectSplit(text1, text2, x1, y1, deadline);
          }
        }
      }
    }
  }
  delete [] v1;
  delete [] v2;
  // Diff took too long and hit the deadline or
  // number of diffs equals number of characters, no commonality at all.
  std::vector<Diff> diffs;
  diffs.push_back(Diff(DELETE, text1));
  diffs.push_back(Diff(INSERT, text2));
  return diffs;
}

std::vector<Diff> diff_match_patch::diff_bisectSplit(const std::string &text1,
    const std::string &text2, int x, int y, clock_t deadline) {
  std::string text1a = text1.substr(0, x);
  std::string text2a = text2.substr(0, y);
  std::string text1b = safeMid(text1, x);
  std::string text2b = safeMid(text2, y);

  // Compute both diffs serially.
  std::vector<Diff> diffs = diff_main(text1a, text2a, false, deadline);
  std::vector<Diff> diffsb = diff_main(text1b, text2b, false, deadline);
  diffs.insert(diffs.end(), diffsb.begin(), diffsb.end());
  return diffs;
}


// Internal u32string versions of bisect/diff for line-mode

std::vector<Diff> diff_match_patch::diff_main_u32(const std::u32string &text1,
    const std::u32string &text2, bool checklines, clock_t deadline, int depth) {
  // Guard against stack overflow from deep recursion: fall back to simple diff.
  if (depth > 100) {
    std::vector<Diff> diffs;
    if (!text1.empty()) {
      std::string enc1;
      for (char32_t c : text1) {
        enc1 += (char)((c >> 24) & 0xFF);
        enc1 += (char)((c >> 16) & 0xFF);
        enc1 += (char)((c >> 8) & 0xFF);
        enc1 += (char)(c & 0xFF);
      }
      diffs.push_back(Diff(DELETE, enc1));
    }
    if (!text2.empty()) {
      std::string enc2;
      for (char32_t c : text2) {
        enc2 += (char)((c >> 24) & 0xFF);
        enc2 += (char)((c >> 16) & 0xFF);
        enc2 += (char)((c >> 8) & 0xFF);
        enc2 += (char)(c & 0xFF);
      }
      diffs.push_back(Diff(INSERT, enc2));
    }
    return diffs;
  }

  std::vector<Diff> diffs;
  if (text1 == text2) {
    if (!text1.empty()) {
      // Encode equal u32string as 4 bytes per char32_t.
      std::string encoded;
      for (char32_t c : text1) {
        encoded += (char)((c >> 24) & 0xFF);
        encoded += (char)((c >> 16) & 0xFF);
        encoded += (char)((c >> 8) & 0xFF);
        encoded += (char)(c & 0xFF);
      }
      diffs.push_back(Diff(EQUAL, encoded));
    }
    return diffs;
  }
  return diff_compute_u32(text1, text2, checklines, deadline, depth);
}

std::vector<Diff> diff_match_patch::diff_compute_u32(std::u32string text1, std::u32string text2,
    bool checklines, clock_t deadline, int depth) {
  std::vector<Diff> diffs;

  if (text1.empty()) {
    // Build placeholder diffs — text will be filled in by charsToLines
    diffs.push_back(Diff(INSERT, ""));
    // Store u32string content temporarily as raw bytes in the text field
    // We'll handle this differently: store char indices as placeholder
    // Actually just build diffs with empty text; charsToLines fills them
    // Better: encode each char32_t as its index back to a string
    // We need to store the u32string so charsToLines can decode it.
    // Use a trick: store u32 content as bytes in the string (4 bytes per char)
    // Actually the simplest approach: build Diff with a marker string
    // and let diff_charsToLines_u32 decode it properly.
    // We'll encode u32string as std::string with each char32_t as 4 bytes.
    std::string encoded;
    for (char32_t c : text2) {
      encoded += (char)((c >> 24) & 0xFF);
      encoded += (char)((c >> 16) & 0xFF);
      encoded += (char)((c >> 8) & 0xFF);
      encoded += (char)(c & 0xFF);
    }
    diffs[0].text = encoded;
    return diffs;
  }

  if (text2.empty()) {
    std::string encoded;
    for (char32_t c : text1) {
      encoded += (char)((c >> 24) & 0xFF);
      encoded += (char)((c >> 16) & 0xFF);
      encoded += (char)((c >> 8) & 0xFF);
      encoded += (char)(c & 0xFF);
    }
    diffs.push_back(Diff(DELETE, encoded));
    return diffs;
  }

  // Single-character shorttext: no common substring possible.
  if (text1.size() == 1 || text2.size() == 1) {
    std::string enc1, enc2;
    for (char32_t c : text1) {
      enc1 += (char)((c >> 24) & 0xFF);
      enc1 += (char)((c >> 16) & 0xFF);
      enc1 += (char)((c >> 8) & 0xFF);
      enc1 += (char)(c & 0xFF);
    }
    for (char32_t c : text2) {
      enc2 += (char)((c >> 24) & 0xFF);
      enc2 += (char)((c >> 16) & 0xFF);
      enc2 += (char)((c >> 8) & 0xFF);
      enc2 += (char)(c & 0xFF);
    }
    diffs.push_back(Diff(DELETE, enc1));
    diffs.push_back(Diff(INSERT, enc2));
    return diffs;
  }

  return diff_bisect_u32(text1, text2, deadline, depth);
}

std::vector<Diff> diff_match_patch::diff_bisect_u32(const std::u32string &text1,
    const std::u32string &text2, clock_t deadline, int depth) {
  const int text1_length = (int)text1.size();
  const int text2_length = (int)text2.size();
  const int max_d = (text1_length + text2_length + 1) / 2;
  const int v_offset = max_d;
  const int v_length = 2 * max_d;
  // Allocate +2 so v1[v_offset+1] is always in bounds even when max_d == 1.
  int *v1 = new int[v_length + 2];
  int *v2 = new int[v_length + 2];
  for (int x = 0; x < v_length; x++) {
    v1[x] = -1;
    v2[x] = -1;
  }
  v1[v_offset + 1] = 0;
  v2[v_offset + 1] = 0;
  const int delta = text1_length - text2_length;
  const bool front = (delta % 2 != 0);
  int k1start = 0;
  int k1end = 0;
  int k2start = 0;
  int k2end = 0;
  for (int d = 0; d < max_d; d++) {
    if (clock() > deadline) {
      break;
    }
    for (int k1 = -d + k1start; k1 <= d - k1end; k1 += 2) {
      const int k1_offset = v_offset + k1;
      int x1;
      if (k1 == -d || (k1 != d && v1[k1_offset - 1] < v1[k1_offset + 1])) {
        x1 = v1[k1_offset + 1];
      } else {
        x1 = v1[k1_offset - 1] + 1;
      }
      int y1 = x1 - k1;
      while (x1 < text1_length && y1 < text2_length
          && text1[x1] == text2[y1]) {
        x1++;
        y1++;
      }
      v1[k1_offset] = x1;
      if (x1 > text1_length) {
        k1end += 2;
      } else if (y1 > text2_length) {
        k1start += 2;
      } else if (front) {
        int k2_offset = v_offset + delta - k1;
        if (k2_offset >= 0 && k2_offset < v_length && v2[k2_offset] != -1) {
          int x2 = text1_length - v2[k2_offset];
          if (x1 >= x2) {
            delete [] v1;
            delete [] v2;
            return diff_bisectSplit_u32(text1, text2, x1, y1, deadline, depth);
          }
        }
      }
    }
    for (int k2 = -d + k2start; k2 <= d - k2end; k2 += 2) {
      const int k2_offset = v_offset + k2;
      int x2;
      if (k2 == -d || (k2 != d && v2[k2_offset - 1] < v2[k2_offset + 1])) {
        x2 = v2[k2_offset + 1];
      } else {
        x2 = v2[k2_offset - 1] + 1;
      }
      int y2 = x2 - k2;
      while (x2 < text1_length && y2 < text2_length
          && text1[text1_length - x2 - 1] == text2[text2_length - y2 - 1]) {
        x2++;
        y2++;
      }
      v2[k2_offset] = x2;
      if (x2 > text1_length) {
        k2end += 2;
      } else if (y2 > text2_length) {
        k2start += 2;
      } else if (!front) {
        int k1_offset = v_offset + delta - k2;
        if (k1_offset >= 0 && k1_offset < v_length && v1[k1_offset] != -1) {
          int x1 = v1[k1_offset];
          int y1 = v_offset + x1 - k1_offset;
          x2 = text1_length - x2;
          if (x1 >= x2) {
            delete [] v1;
            delete [] v2;
            return diff_bisectSplit_u32(text1, text2, x1, y1, deadline, depth);
          }
        }
      }
    }
  }
  delete [] v1;
  delete [] v2;
  // Timeout or no commonality.
  // Encode u32strings as std::string (4 bytes per char32_t)
  std::vector<Diff> diffs;
  std::string enc1, enc2;
  for (char32_t c : text1) {
    enc1 += (char)((c >> 24) & 0xFF);
    enc1 += (char)((c >> 16) & 0xFF);
    enc1 += (char)((c >> 8) & 0xFF);
    enc1 += (char)(c & 0xFF);
  }
  for (char32_t c : text2) {
    enc2 += (char)((c >> 24) & 0xFF);
    enc2 += (char)((c >> 16) & 0xFF);
    enc2 += (char)((c >> 8) & 0xFF);
    enc2 += (char)(c & 0xFF);
  }
  diffs.push_back(Diff(DELETE, enc1));
  diffs.push_back(Diff(INSERT, enc2));
  return diffs;
}

std::vector<Diff> diff_match_patch::diff_bisectSplit_u32(const std::u32string &text1,
    const std::u32string &text2, int x, int y, clock_t deadline, int depth) {
  std::u32string text1a = text1.substr(0, x);
  std::u32string text2a = text2.substr(0, y);
  std::u32string text1b = text1.substr(x);
  std::u32string text2b = text2.substr(y);

  std::vector<Diff> diffs = diff_main_u32(text1a, text2a, false, deadline, depth + 1);
  std::vector<Diff> diffsb = diff_main_u32(text1b, text2b, false, deadline, depth + 1);
  diffs.insert(diffs.end(), diffsb.begin(), diffsb.end());
  return diffs;
}


LinesToCharsResult diff_match_patch::diff_linesToChars(const std::string &text1,
                                                        const std::string &text2) {
  std::vector<std::string> lineArray;
  std::map<std::string, int> lineHash;

  // "\x00" is a valid character, but various debuggers don't like it.
  // So we'll insert a junk entry to avoid generating a null character.
  lineArray.push_back("");

  const std::u32string chars1 = diff_linesToCharsMunge(text1, lineArray, lineHash);
  const std::u32string chars2 = diff_linesToCharsMunge(text2, lineArray, lineHash);

  LinesToCharsResult result;
  result.chars1 = chars1;
  result.chars2 = chars2;
  result.lineArray = lineArray;
  return result;
}


std::u32string diff_match_patch::diff_linesToCharsMunge(const std::string &text,
                                                         std::vector<std::string> &lineArray,
                                                         std::map<std::string, int> &lineHash) {
  int lineStart = 0;
  int lineEnd = -1;
  std::string line;
  std::u32string chars;
  // Walk the text, pulling out a substring for each line.
  while (lineEnd < (int)text.size() - 1) {
    std::size_t found = text.find('\n', lineStart);
    if (found == std::string::npos) {
      lineEnd = (int)text.size() - 1;
    } else {
      lineEnd = (int)found;
    }
    line = safeMid(text, lineStart, lineEnd + 1 - lineStart);
    lineStart = lineEnd + 1;

    auto it = lineHash.find(line);
    if (it != lineHash.end()) {
      chars += (char32_t)it->second;
    } else {
      lineArray.push_back(line);
      lineHash[line] = (int)lineArray.size() - 1;
      chars += (char32_t)(lineArray.size() - 1);
    }
  }
  return chars;
}


void diff_match_patch::diff_charsToLines(std::vector<Diff> &diffs,
                                          const std::vector<std::string> &lineArray) {
  // Not used directly anymore; kept for API compatibility.
  // The u32 version is used internally.
}

void diff_match_patch::diff_charsToLines_u32(std::vector<Diff> &diffs,
                                              const std::vector<std::string> &lineArray) {
  // Decode the encoded string (4 bytes per char32_t) back to text.
  for (Diff &diff : diffs) {
    std::string text;
    const std::string &enc = diff.text;
    // Each char32_t is stored as 4 bytes big-endian.
    for (std::size_t j = 0; j + 3 < enc.size(); j += 4) {
      char32_t idx = ((char32_t)(unsigned char)enc[j] << 24)
                   | ((char32_t)(unsigned char)enc[j+1] << 16)
                   | ((char32_t)(unsigned char)enc[j+2] << 8)
                   | ((char32_t)(unsigned char)enc[j+3]);
      if (idx < (char32_t)lineArray.size()) {
        text += lineArray[(int)idx];
      }
    }
    diff.text = text;
  }
}


int diff_match_patch::diff_commonPrefix(const std::string &text1,
                                        const std::string &text2) {
  // Performance analysis: http://neil.fraser.name/news/2007/10/09/
  const int n = std::min((int)text1.size(), (int)text2.size());
  for (int i = 0; i < n; i++) {
    if (text1[i] != text2[i]) {
      return i;
    }
  }
  return n;
}


int diff_match_patch::diff_commonSuffix(const std::string &text1,
                                        const std::string &text2) {
  // Performance analysis: http://neil.fraser.name/news/2007/10/09/
  const int text1_length = (int)text1.size();
  const int text2_length = (int)text2.size();
  const int n = std::min(text1_length, text2_length);
  for (int i = 1; i <= n; i++) {
    if (text1[text1_length - i] != text2[text2_length - i]) {
      return i - 1;
    }
  }
  return n;
}

int diff_match_patch::diff_commonOverlap(const std::string &text1,
                                          const std::string &text2) {
  // Cache the text lengths to prevent multiple calls.
  const int text1_length = (int)text1.size();
  const int text2_length = (int)text2.size();
  // Eliminate the null case.
  if (text1_length == 0 || text2_length == 0) {
    return 0;
  }
  // Truncate the longer string.
  std::string text1_trunc = text1;
  std::string text2_trunc = text2;
  if (text1_length > text2_length) {
    text1_trunc = text1.substr(text1_length - text2_length);
  } else if (text1_length < text2_length) {
    text2_trunc = text2.substr(0, text1_length);
  }
  const int text_length = std::min(text1_length, text2_length);
  // Quick check for the worst case.
  if (text1_trunc == text2_trunc) {
    return text_length;
  }

  // Start by looking for a single character match
  // and increase length until no match is found.
  // Performance analysis: http://neil.fraser.name/news/2010/11/04/
  int best = 0;
  int length = 1;
  while (true) {
    std::string pattern = text1_trunc.substr(text1_trunc.size() - length);
    std::size_t found = text2_trunc.find(pattern);
    if (found == std::string::npos) {
      return best;
    }
    length += (int)found;
    if (found == 0 || text1_trunc.substr(text1_trunc.size() - length) == text2_trunc.substr(0, length)) {
      best = length;
      length++;
    }
  }
}

std::vector<std::string> diff_match_patch::diff_halfMatch(const std::string &text1,
                                                            const std::string &text2) {
  if (Diff_Timeout <= 0) {
    // Don't risk returning a non-optimal diff if we have unlimited time.
    return std::vector<std::string>();
  }
  const std::string longtext = text1.size() > text2.size() ? text1 : text2;
  const std::string shorttext = text1.size() > text2.size() ? text2 : text1;
  if (longtext.size() < 4 || shorttext.size() * 2 < longtext.size()) {
    return std::vector<std::string>();  // Pointless.
  }

  // First check if the second quarter is the seed for a half-match.
  const std::vector<std::string> hm1 = diff_halfMatchI(longtext, shorttext,
      (int)(longtext.size() + 3) / 4);
  // Check again based on the third quarter.
  const std::vector<std::string> hm2 = diff_halfMatchI(longtext, shorttext,
      (int)(longtext.size() + 1) / 2);
  std::vector<std::string> hm;
  if (hm1.empty() && hm2.empty()) {
    return std::vector<std::string>();
  } else if (hm2.empty()) {
    hm = hm1;
  } else if (hm1.empty()) {
    hm = hm2;
  } else {
    // Both matched.  Select the longest.
    hm = hm1[4].size() > hm2[4].size() ? hm1 : hm2;
  }

  // A half-match was found, sort out the return data.
  if (text1.size() > text2.size()) {
    return hm;
  } else {
    std::vector<std::string> listRet;
    listRet.push_back(hm[2]);
    listRet.push_back(hm[3]);
    listRet.push_back(hm[0]);
    listRet.push_back(hm[1]);
    listRet.push_back(hm[4]);
    return listRet;
  }
}


std::vector<std::string> diff_match_patch::diff_halfMatchI(const std::string &longtext,
                                                             const std::string &shorttext,
                                                             int i) {
  // Start with a 1/4 length substring at position i as a seed.
  const std::string seed = safeMid(longtext, i, (int)longtext.size() / 4);
  int j = -1;
  std::string best_common;
  std::string best_longtext_a, best_longtext_b;
  std::string best_shorttext_a, best_shorttext_b;

  std::size_t found = shorttext.find(seed, j + 1);
  while (found != std::string::npos) {
    j = (int)found;
    const int prefixLength = diff_commonPrefix(safeMid(longtext, i),
        safeMid(shorttext, j));
    const int suffixLength = diff_commonSuffix(longtext.substr(0, i),
        shorttext.substr(0, j));
    if ((int)best_common.size() < suffixLength + prefixLength) {
      best_common = safeMid(shorttext, j - suffixLength, suffixLength)
          + safeMid(shorttext, j, prefixLength);
      best_longtext_a = longtext.substr(0, i - suffixLength);
      best_longtext_b = safeMid(longtext, i + prefixLength);
      best_shorttext_a = shorttext.substr(0, j - suffixLength);
      best_shorttext_b = safeMid(shorttext, j + prefixLength);
    }
    found = shorttext.find(seed, j + 1);
  }
  if (best_common.size() * 2 >= longtext.size()) {
    std::vector<std::string> listRet;
    listRet.push_back(best_longtext_a);
    listRet.push_back(best_longtext_b);
    listRet.push_back(best_shorttext_a);
    listRet.push_back(best_shorttext_b);
    listRet.push_back(best_common);
    return listRet;
  } else {
    return std::vector<std::string>();
  }
}


void diff_match_patch::diff_cleanupSemantic(std::vector<Diff> &diffs) {
  if (diffs.empty()) {
    return;
  }
  bool changes = false;
  std::stack<Diff> equalities;  // Stack of equalities.
  std::string lastequality;  // Always equal to equalities.top().text
  bool lastequality_valid = false;
  int pointer = 0;  // index into diffs
  // Number of characters that changed prior to the equality.
  int length_insertions1 = 0;
  int length_deletions1 = 0;
  // Number of characters that changed after the equality.
  int length_insertions2 = 0;
  int length_deletions2 = 0;

  while (pointer < (int)diffs.size()) {
    if (diffs[pointer].operation == EQUAL) {
      // Equality found.
      equalities.push(diffs[pointer]);
      length_insertions1 = length_insertions2;
      length_deletions1 = length_deletions2;
      length_insertions2 = 0;
      length_deletions2 = 0;
      lastequality = diffs[pointer].text;
      lastequality_valid = true;
    } else {
      // An insertion or deletion.
      if (diffs[pointer].operation == INSERT) {
        length_insertions2 += (int)diffs[pointer].text.size();
      } else {
        length_deletions2 += (int)diffs[pointer].text.size();
      }
      // Eliminate an equality that is smaller or equal to the edits on both
      // sides of it.
      if (lastequality_valid
          && ((int)lastequality.size()
              <= std::max(length_insertions1, length_deletions1))
          && ((int)lastequality.size()
              <= std::max(length_insertions2, length_deletions2))) {
        // Walk back to offending equality.
        int eq_idx = pointer - 1;
        while (eq_idx >= 0 && diffs[eq_idx] != equalities.top()) {
          eq_idx--;
        }

        // Replace equality with a delete.
        diffs[eq_idx] = Diff(DELETE, lastequality);
        // Insert a corresponding insert after eq_idx.
        diffs.insert(diffs.begin() + eq_idx + 1, Diff(INSERT, lastequality));
        // pointer shifts by 1 due to insertion.
        pointer++;

        equalities.pop();  // Throw away the equality we just deleted.
        if (!equalities.empty()) {
          // Throw away the previous equality (it needs to be reevaluated).
          equalities.pop();
        }
        if (equalities.empty()) {
          // There are no previous equalities, walk back to the start.
          pointer = 0;
        } else {
          // There is a safe equality we can fall back to.
          // Find equalities.top() in diffs.
          int back_idx = pointer - 1;
          while (back_idx >= 0 && diffs[back_idx] != equalities.top()) {
            back_idx--;
          }
          pointer = back_idx + 1;
        }

        length_insertions1 = 0;  // Reset the counters.
        length_deletions1 = 0;
        length_insertions2 = 0;
        length_deletions2 = 0;
        lastequality_valid = false;
        changes = true;
        continue;
      }
    }
    pointer++;
  }

  // Normalize the diff.
  if (changes) {
    diff_cleanupMerge(diffs);
  }
  diff_cleanupSemanticLossless(diffs);

  // Find any overlaps between deletions and insertions.
  pointer = 1;
  while (pointer < (int)diffs.size()) {
    if (diffs[pointer - 1].operation == DELETE &&
        diffs[pointer].operation == INSERT) {
      std::string deletion = diffs[pointer - 1].text;
      std::string insertion = diffs[pointer].text;
      int overlap_length1 = diff_commonOverlap(deletion, insertion);
      int overlap_length2 = diff_commonOverlap(insertion, deletion);
      if (overlap_length1 >= overlap_length2) {
        if (overlap_length1 >= (int)deletion.size() / 2.0 ||
            overlap_length1 >= (int)insertion.size() / 2.0) {
          // Overlap found.  Insert an equality and trim the surrounding edits.
          diffs.insert(diffs.begin() + pointer,
              Diff(EQUAL, insertion.substr(0, overlap_length1)));
          diffs[pointer - 1].text = deletion.substr(0, deletion.size() - overlap_length1);
          diffs[pointer + 1].text = safeMid(insertion, overlap_length1);
          pointer++;
        }
      } else {
        if (overlap_length2 >= (int)deletion.size() / 2.0 ||
            overlap_length2 >= (int)insertion.size() / 2.0) {
          // Reverse overlap found.
          // Insert an equality and swap and trim the surrounding edits.
          diffs.insert(diffs.begin() + pointer,
              Diff(EQUAL, deletion.substr(0, overlap_length2)));
          diffs[pointer - 1].operation = INSERT;
          diffs[pointer - 1].text = insertion.substr(0, insertion.size() - overlap_length2);
          diffs[pointer + 1].operation = DELETE;
          diffs[pointer + 1].text = safeMid(deletion, overlap_length2);
          pointer++;
        }
      }
      pointer++;
    }
    pointer++;
  }
}


void diff_match_patch::diff_cleanupSemanticLossless(std::vector<Diff> &diffs) {
  // Intentionally ignore the first and last element (don't need checking).
  int pointer = 1;
  while (pointer < (int)diffs.size() - 1) {
    if (diffs[pointer - 1].operation == EQUAL &&
        diffs[pointer + 1].operation == EQUAL) {
      // This is a single edit surrounded by equalities.
      std::string equality1 = diffs[pointer - 1].text;
      std::string edit = diffs[pointer].text;
      std::string equality2 = diffs[pointer + 1].text;

      // First, shift the edit as far left as possible.
      int commonOffset = diff_commonSuffix(equality1, edit);
      if (commonOffset != 0) {
        std::string commonString = edit.substr(edit.size() - commonOffset);
        equality1 = equality1.substr(0, equality1.size() - commonOffset);
        edit = commonString + edit.substr(0, edit.size() - commonOffset);
        equality2 = commonString + equality2;
      }

      // Second, step character by character right, looking for the best fit.
      std::string bestEquality1 = equality1;
      std::string bestEdit = edit;
      std::string bestEquality2 = equality2;
      int bestScore = diff_cleanupSemanticScore(equality1, edit)
          + diff_cleanupSemanticScore(edit, equality2);
      while (!edit.empty() && !equality2.empty()
          && edit[0] == equality2[0]) {
        equality1 += edit[0];
        edit = edit.substr(1) + equality2[0];
        equality2 = equality2.substr(1);
        int score = diff_cleanupSemanticScore(equality1, edit)
            + diff_cleanupSemanticScore(edit, equality2);
        // The >= encourages trailing rather than leading whitespace on edits.
        if (score >= bestScore) {
          bestScore = score;
          bestEquality1 = equality1;
          bestEdit = edit;
          bestEquality2 = equality2;
        }
      }

      if (diffs[pointer - 1].text != bestEquality1) {
        // We have an improvement, save it back to the diff.
        if (!bestEquality1.empty()) {
          diffs[pointer - 1].text = bestEquality1;
        } else {
          diffs.erase(diffs.begin() + pointer - 1);
          pointer--;
        }
        diffs[pointer].text = bestEdit;
        if (!bestEquality2.empty()) {
          diffs[pointer + 1].text = bestEquality2;
        } else {
          diffs.erase(diffs.begin() + pointer + 1);
          // Don't increment pointer.
          continue;
        }
      }
    }
    pointer++;
  }
}


int diff_match_patch::diff_cleanupSemanticScore(const std::string &one,
                                                 const std::string &two) {
  if (one.empty() || two.empty()) {
    // Edges are the best.
    return 6;
  }

  // Each port of this function behaves slightly differently due to
  // subtle differences in each language's definition of things like
  // 'whitespace'.  Since this function's purpose is largely cosmetic,
  // the choice has been made to use each language's native features
  // rather than force total conformity.
  char char1 = one[one.size() - 1];
  char char2 = two[0];
  bool nonAlphaNumeric1 = !std::isalnum((unsigned char)char1);
  bool nonAlphaNumeric2 = !std::isalnum((unsigned char)char2);
  bool whitespace1 = nonAlphaNumeric1 && std::isspace((unsigned char)char1);
  bool whitespace2 = nonAlphaNumeric2 && std::isspace((unsigned char)char2);
  bool lineBreak1 = whitespace1 && (char1 == '\r' || char1 == '\n');
  bool lineBreak2 = whitespace2 && (char2 == '\r' || char2 == '\n');
  bool blankLine1 = lineBreak1 && std::regex_search(one, BLANKLINEEND);
  bool blankLine2 = lineBreak2 && std::regex_search(two, BLANKLINESTART);

  if (blankLine1 || blankLine2) {
    // Five points for blank lines.
    return 5;
  } else if (lineBreak1 || lineBreak2) {
    // Four points for line breaks.
    return 4;
  } else if (nonAlphaNumeric1 && !whitespace1 && whitespace2) {
    // Three points for end of sentences.
    return 3;
  } else if (whitespace1 || whitespace2) {
    // Two points for whitespace.
    return 2;
  } else if (nonAlphaNumeric1 || nonAlphaNumeric2) {
    // One point for non-alphanumeric.
    return 1;
  }
  return 0;
}


void diff_match_patch::diff_cleanupEfficiency(std::vector<Diff> &diffs) {
  if (diffs.empty()) {
    return;
  }
  bool changes = false;
  std::stack<Diff> equalities;  // Stack of equalities.
  std::string lastequality;  // Always equal to equalities.top().text
  bool lastequality_valid = false;
  // Is there an insertion operation before the last equality.
  bool pre_ins = false;
  // Is there a deletion operation before the last equality.
  bool pre_del = false;
  // Is there an insertion operation after the last equality.
  bool post_ins = false;
  // Is there a deletion operation after the last equality.
  bool post_del = false;

  int pointer = 0;
  int safe_pointer = 0;  // index of safeDiff

  while (pointer < (int)diffs.size()) {
    if (diffs[pointer].operation == EQUAL) {
      // Equality found.
      if ((int)diffs[pointer].text.size() < Diff_EditCost && (post_ins || post_del)) {
        // Candidate found.
        equalities.push(diffs[pointer]);
        pre_ins = post_ins;
        pre_del = post_del;
        lastequality = diffs[pointer].text;
        lastequality_valid = true;
      } else {
        // Not a candidate, and can never become one.
        while (!equalities.empty()) equalities.pop();
        lastequality_valid = false;
        safe_pointer = pointer;
      }
      post_ins = post_del = false;
    } else {
      // An insertion or deletion.
      if (diffs[pointer].operation == DELETE) {
        post_del = true;
      } else {
        post_ins = true;
      }
      if (lastequality_valid
          && ((pre_ins && pre_del && post_ins && post_del)
          || (((int)lastequality.size() < Diff_EditCost / 2)
          && ((pre_ins ? 1 : 0) + (pre_del ? 1 : 0)
          + (post_ins ? 1 : 0) + (post_del ? 1 : 0)) == 3))) {
        // Walk back to offending equality.
        int eq_idx = pointer - 1;
        while (eq_idx >= 0 && diffs[eq_idx] != equalities.top()) {
          eq_idx--;
        }
        // Replace equality with a delete.
        diffs[eq_idx] = Diff(DELETE, lastequality);
        // Insert a corresponding insert.
        diffs.insert(diffs.begin() + eq_idx + 1, Diff(INSERT, lastequality));
        pointer++;  // Account for inserted element.

        equalities.pop();  // Throw away the equality we just deleted.
        lastequality_valid = false;
        if (pre_ins && pre_del) {
          // No changes made which could affect previous entry, keep going.
          post_ins = post_del = true;
          while (!equalities.empty()) equalities.pop();
          safe_pointer = pointer;
        } else {
          if (!equalities.empty()) {
            equalities.pop();
          }
          if (equalities.empty()) {
            pointer = safe_pointer;
          } else {
            // Find equalities.top() in diffs.
            int back_idx = pointer - 1;
            while (back_idx >= 0 && diffs[back_idx] != equalities.top()) {
              back_idx--;
            }
            pointer = back_idx + 1;
          }
          post_ins = post_del = false;
        }
        changes = true;
        continue;
      }
    }
    pointer++;
  }

  if (changes) {
    diff_cleanupMerge(diffs);
  }
}


void diff_match_patch::diff_cleanupMerge(std::vector<Diff> &diffs) {
  diffs.push_back(Diff(EQUAL, ""));  // Add a dummy entry at the end.
  int pointer = 0;
  int count_delete = 0;
  int count_insert = 0;
  std::string text_delete;
  std::string text_insert;
  int prevEqual = -1;  // index of previous equal diff, or -1
  int commonlength;

  while (pointer < (int)diffs.size()) {
    switch (diffs[pointer].operation) {
      case INSERT:
        count_insert++;
        text_insert += diffs[pointer].text;
        prevEqual = -1;
        pointer++;
        break;
      case DELETE:
        count_delete++;
        text_delete += diffs[pointer].text;
        prevEqual = -1;
        pointer++;
        break;
      case EQUAL:
        if (count_delete + count_insert > 1) {
          bool both_types = count_delete != 0 && count_insert != 0;
          // Delete the offending records.
          int start = pointer - count_delete - count_insert;
          diffs.erase(diffs.begin() + start, diffs.begin() + pointer);
          pointer = start;

          if (both_types) {
            // Factor out any common prefixes.
            commonlength = diff_commonPrefix(text_insert, text_delete);
            if (commonlength != 0) {
              if (pointer > 0) {
                if (diffs[pointer - 1].operation != EQUAL) {
                  throw "Previous diff should have been an equality.";
                }
                diffs[pointer - 1].text += text_insert.substr(0, commonlength);
              } else {
                diffs.insert(diffs.begin() + pointer, Diff(EQUAL, text_insert.substr(0, commonlength)));
                pointer++;
              }
              text_insert = text_insert.substr(commonlength);
              text_delete = text_delete.substr(commonlength);
            }
            // Factor out any common suffixes.
            commonlength = diff_commonSuffix(text_insert, text_delete);
            if (commonlength != 0) {
              diffs[pointer].text = text_insert.substr(text_insert.size() - commonlength) + diffs[pointer].text;
              text_insert = text_insert.substr(0, text_insert.size() - commonlength);
              text_delete = text_delete.substr(0, text_delete.size() - commonlength);
            }
          }
          // Insert the merged records.
          if (!text_delete.empty()) {
            diffs.insert(diffs.begin() + pointer, Diff(DELETE, text_delete));
            pointer++;
          }
          if (!text_insert.empty()) {
            diffs.insert(diffs.begin() + pointer, Diff(INSERT, text_insert));
            pointer++;
          }
          // Step forward to the equality.
          prevEqual = pointer;
          pointer++;
        } else if (prevEqual >= 0) {
          // Merge this equality with the previous one.
          diffs[prevEqual].text += diffs[pointer].text;
          diffs.erase(diffs.begin() + pointer);
          // Don't increment pointer.
        } else {
          prevEqual = pointer;
          pointer++;
        }
        count_insert = 0;
        count_delete = 0;
        text_delete = "";
        text_insert = "";
        break;
    }
  }
  if (diffs.back().text.empty()) {
    diffs.pop_back();  // Remove the dummy entry at the end.
  }

  /*
  * Second pass: look for single edits surrounded on both sides by equalities
  * which can be shifted sideways to eliminate an equality.
  * e.g: A<ins>BA</ins>C -> <ins>AB</ins>AC
  */
  bool changes = false;
  pointer = 1;
  while (pointer < (int)diffs.size() - 1) {
    if (diffs[pointer - 1].operation == EQUAL &&
        diffs[pointer + 1].operation == EQUAL) {
      // This is a single edit surrounded by equalities.
      if (strEndsWith(diffs[pointer].text, diffs[pointer - 1].text)) {
        // Shift the edit over the previous equality.
        if (!diffs[pointer - 1].text.empty()) {
          diffs[pointer].text = diffs[pointer - 1].text
              + diffs[pointer].text.substr(0, diffs[pointer].text.size()
              - diffs[pointer - 1].text.size());
          diffs[pointer + 1].text = diffs[pointer - 1].text + diffs[pointer + 1].text;
          diffs.erase(diffs.begin() + pointer - 1);
          // pointer is now at the edit; nextDiff is pointer+1
          pointer++;
          changes = true;
        } else {
          pointer++;
        }
      } else if (strStartsWith(diffs[pointer].text, diffs[pointer + 1].text)) {
        // Shift the edit over the next equality.
        diffs[pointer - 1].text += diffs[pointer + 1].text;
        diffs[pointer].text = diffs[pointer].text.substr(diffs[pointer + 1].text.size())
            + diffs[pointer + 1].text;
        diffs.erase(diffs.begin() + pointer + 1);
        changes = true;
        pointer++;
      } else {
        pointer++;
      }
    } else {
      pointer++;
    }
  }
  // If shifts were made, the diff needs reordering and another shift sweep.
  if (changes) {
    diff_cleanupMerge(diffs);
  }
}


int diff_match_patch::diff_xIndex(const std::vector<Diff> &diffs, int loc) {
  int chars1 = 0;
  int chars2 = 0;
  int last_chars1 = 0;
  int last_chars2 = 0;
  Diff lastDiff;
  for (const Diff &aDiff : diffs) {
    if (aDiff.operation != INSERT) {
      // Equality or deletion.
      chars1 += (int)aDiff.text.size();
    }
    if (aDiff.operation != DELETE) {
      // Equality or insertion.
      chars2 += (int)aDiff.text.size();
    }
    if (chars1 > loc) {
      // Overshot the location.
      lastDiff = aDiff;
      break;
    }
    last_chars1 = chars1;
    last_chars2 = chars2;
  }
  if (lastDiff.operation == DELETE) {
    // The location was deleted.
    return last_chars2;
  }
  // Add the remaining character length.
  return last_chars2 + (loc - last_chars1);
}


std::string diff_match_patch::diff_prettyHtml(const std::vector<Diff> &diffs) {
  std::string html;
  for (const Diff &aDiff : diffs) {
    std::string text = aDiff.text;
    text = replaceAll(text, "&", "&amp;");
    text = replaceAll(text, "<", "&lt;");
    text = replaceAll(text, ">", "&gt;");
    text = replaceAll(text, "\n", "&para;<br>");
    switch (aDiff.operation) {
      case INSERT:
        html += std::string("<ins style=\"background:#e6ffe6;\">") + text
            + std::string("</ins>");
        break;
      case DELETE:
        html += std::string("<del style=\"background:#ffe6e6;\">") + text
            + std::string("</del>");
        break;
      case EQUAL:
        html += std::string("<span>") + text + std::string("</span>");
        break;
    }
  }
  return html;
}


std::string diff_match_patch::diff_text1(const std::vector<Diff> &diffs) {
  std::string text;
  for (const Diff &aDiff : diffs) {
    if (aDiff.operation != INSERT) {
      text += aDiff.text;
    }
  }
  return text;
}


std::string diff_match_patch::diff_text2(const std::vector<Diff> &diffs) {
  std::string text;
  for (const Diff &aDiff : diffs) {
    if (aDiff.operation != DELETE) {
      text += aDiff.text;
    }
  }
  return text;
}


int diff_match_patch::diff_levenshtein(const std::vector<Diff> &diffs) {
  int levenshtein = 0;
  int insertions = 0;
  int deletions = 0;
  for (const Diff &aDiff : diffs) {
    switch (aDiff.operation) {
      case INSERT:
        insertions += (int)aDiff.text.size();
        break;
      case DELETE:
        deletions += (int)aDiff.text.size();
        break;
      case EQUAL:
        // A deletion and an insertion is one substitution.
        levenshtein += std::max(insertions, deletions);
        insertions = 0;
        deletions = 0;
        break;
    }
  }
  levenshtein += std::max(insertions, deletions);
  return levenshtein;
}


std::string diff_match_patch::diff_toDelta(const std::vector<Diff> &diffs) {
  std::string text;
  for (const Diff &aDiff : diffs) {
    switch (aDiff.operation) {
      case INSERT: {
        std::string encoded = percentEncode(aDiff.text, " !~*'();/?:@&=+$,#");
        text += std::string("+") + encoded + std::string("\t");
        break;
      }
      case DELETE:
        text += std::string("-") + std::to_string((int)aDiff.text.size())
            + std::string("\t");
        break;
      case EQUAL:
        text += std::string("=") + std::to_string((int)aDiff.text.size())
            + std::string("\t");
        break;
    }
  }
  if (!text.empty()) {
    // Strip off trailing tab character.
    text = text.substr(0, text.size() - 1);
  }
  return text;
}


std::vector<Diff> diff_match_patch::diff_fromDelta(const std::string &text1,
                                                    const std::string &delta) {
  std::vector<Diff> diffs;
  int pointer = 0;  // Cursor in text1
  std::vector<std::string> tokens = splitString(delta, '\t');
  for (const std::string &token : tokens) {
    if (token.empty()) {
      // Blank tokens are ok (from a trailing \t).
      continue;
    }
    // Each token begins with a one character parameter which specifies the
    // operation of this token (delete, insert, equality).
    std::string param = token.substr(1);
    switch (token[0]) {
      case '+':
        param = replaceAll(param, "+", "%2B");
        param = percentDecode(param);
        diffs.push_back(Diff(INSERT, param));
        break;
      case '-':
        // Fall through.
      case '=': {
        int n;
        try {
          n = std::stoi(param);
        } catch (...) {
          throw std::string("Invalid number in diff_fromDelta: ") + param;
        }
        if (n < 0) {
          throw std::string("Negative number in diff_fromDelta: ") + param;
        }
        std::string text;
        text = safeMid(text1, pointer, n);
        pointer += n;
        if (token[0] == '=') {
          diffs.push_back(Diff(EQUAL, text));
        } else {
          diffs.push_back(Diff(DELETE, text));
        }
        break;
      }
      default:
        throw std::string("Invalid diff operation in diff_fromDelta: ")
            + token[0];
    }
  }
  if (pointer != (int)text1.size()) {
    throw std::string("Delta length (") + std::to_string(pointer)
        + std::string(") smaller than source text length (")
        + std::to_string((int)text1.size()) + std::string(")");
  }
  return diffs;
}


  //  MATCH FUNCTIONS


int diff_match_patch::match_main(const std::string &text, const std::string &pattern,
                                  int loc) {
  loc = std::max(0, std::min(loc, (int)text.size()));
  if (text == pattern) {
    // Shortcut (potentially not guaranteed by the algorithm)
    return 0;
  } else if (text.empty()) {
    // Nothing to match.
    return -1;
  } else if (loc + (int)pattern.size() <= (int)text.size()
      && safeMid(text, loc, (int)pattern.size()) == pattern) {
    // Perfect match at the perfect spot!  (Includes case of null pattern)
    return loc;
  } else {
    // Do a fuzzy compare.
    return match_bitap(text, pattern, loc);
  }
}


int diff_match_patch::match_bitap(const std::string &text, const std::string &pattern,
                                   int loc) {
  if (!(Match_MaxBits == 0 || (int)pattern.size() <= Match_MaxBits)) {
    throw "Pattern too long for this application.";
  }

  // Initialise the alphabet.
  std::map<char, int> s = match_alphabet(pattern);

  // Highest score beyond which we give up.
  double score_threshold = Match_Threshold;
  // Is there a nearby exact match? (speedup)
  std::size_t best_loc_found = text.find(pattern, loc);
  int best_loc = -1;
  if (best_loc_found != std::string::npos) {
    best_loc = (int)best_loc_found;
    score_threshold = std::min(match_bitapScore(0, best_loc, loc, pattern),
        score_threshold);
    // What about in the other direction? (speedup)
    std::size_t rfound = text.rfind(pattern, loc + (int)pattern.size());
    if (rfound != std::string::npos) {
      best_loc = (int)rfound;
      score_threshold = std::min(match_bitapScore(0, best_loc, loc, pattern),
          score_threshold);
    }
  }

  // Initialise the bit arrays.
  int matchmask = 1 << ((int)pattern.size() - 1);
  best_loc = -1;

  int bin_min, bin_mid;
  int bin_max = (int)pattern.size() + (int)text.size();
  int *rd;
  int *last_rd = nullptr;
  for (int d = 0; d < (int)pattern.size(); d++) {
    // Scan for the best match; each iteration allows for one more error.
    bin_min = 0;
    bin_mid = bin_max;
    while (bin_min < bin_mid) {
      if (match_bitapScore(d, loc + bin_mid, loc, pattern)
          <= score_threshold) {
        bin_min = bin_mid;
      } else {
        bin_max = bin_mid;
      }
      bin_mid = (bin_max - bin_min) / 2 + bin_min;
    }
    // Use the result from this iteration as the maximum for the next.
    bin_max = bin_mid;
    int start = std::max(1, loc - bin_mid + 1);
    int finish = std::min(loc + bin_mid, (int)text.size()) + (int)pattern.size();

    rd = new int[finish + 2];
    rd[finish + 1] = (1 << d) - 1;
    for (int j = finish; j >= start; j--) {
      int charMatch;
      if ((int)text.size() <= j - 1) {
        // Out of range.
        charMatch = 0;
      } else {
        auto it = s.find(text[j - 1]);
        charMatch = (it != s.end()) ? it->second : 0;
      }
      if (d == 0) {
        // First pass: exact match.
        rd[j] = ((rd[j + 1] << 1) | 1) & charMatch;
      } else {
        // Subsequent passes: fuzzy match.
        rd[j] = ((rd[j + 1] << 1) | 1) & charMatch
            | (((last_rd[j + 1] | last_rd[j]) << 1) | 1)
            | last_rd[j + 1];
      }
      if ((rd[j] & matchmask) != 0) {
        double score = match_bitapScore(d, j - 1, loc, pattern);
        if (score <= score_threshold) {
          score_threshold = score;
          best_loc = j - 1;
          if (best_loc > loc) {
            start = std::max(1, 2 * loc - best_loc);
          } else {
            break;
          }
        }
      }
    }
    if (match_bitapScore(d + 1, loc, loc, pattern) > score_threshold) {
      // No hope for a (better) match at greater error levels.
      break;
    }
    delete [] last_rd;
    last_rd = rd;
  }
  delete [] last_rd;
  delete [] rd;
  return best_loc;
}


double diff_match_patch::match_bitapScore(int e, int x, int loc,
                                           const std::string &pattern) {
  const float accuracy = static_cast<float>(e) / (int)pattern.size();
  const int proximity = std::abs(loc - x);
  if (Match_Distance == 0) {
    // Dodge divide by zero error.
    return proximity == 0 ? accuracy : 1.0;
  }
  return accuracy + (proximity / static_cast<float>(Match_Distance));
}


std::map<char, int> diff_match_patch::match_alphabet(const std::string &pattern) {
  std::map<char, int> s;
  int i;
  for (i = 0; i < (int)pattern.size(); i++) {
    char c = pattern[i];
    s[c] = 0;
  }
  for (i = 0; i < (int)pattern.size(); i++) {
    char c = pattern[i];
    s[c] = s[c] | (1 << ((int)pattern.size() - i - 1));
  }
  return s;
}


//  PATCH FUNCTIONS


void diff_match_patch::patch_addContext(Patch &patch, const std::string &text) {
  if (text.empty()) {
    return;
  }
  std::string pattern = safeMid(text, patch.start2, patch.length1);
  int padding = 0;

  // Look for the first and last matches of pattern in text.  If two different
  // matches are found, increase the pattern length.
  while (text.find(pattern) != text.rfind(pattern)
      && (int)pattern.size() < Match_MaxBits - Patch_Margin - Patch_Margin) {
    padding += Patch_Margin;
    pattern = safeMid(text, std::max(0, patch.start2 - padding),
        std::min((int)text.size(), patch.start2 + patch.length1 + padding)
        - std::max(0, patch.start2 - padding));
  }
  // Add one chunk for good luck.
  padding += Patch_Margin;

  // Add the prefix.
  std::string prefix = safeMid(text, std::max(0, patch.start2 - padding),
      patch.start2 - std::max(0, patch.start2 - padding));
  if (!prefix.empty()) {
    patch.diffs.insert(patch.diffs.begin(), Diff(EQUAL, prefix));
  }
  // Add the suffix.
  std::string suffix = safeMid(text, patch.start2 + patch.length1,
      std::min((int)text.size(), patch.start2 + patch.length1 + padding)
      - (patch.start2 + patch.length1));
  if (!suffix.empty()) {
    patch.diffs.push_back(Diff(EQUAL, suffix));
  }

  // Roll back the start points.
  patch.start1 -= (int)prefix.size();
  patch.start2 -= (int)prefix.size();
  // Extend the lengths.
  patch.length1 += (int)prefix.size() + (int)suffix.size();
  patch.length2 += (int)prefix.size() + (int)suffix.size();
}


std::vector<Patch> diff_match_patch::patch_make(const std::string &text1,
                                                  const std::string &text2) {
  // No diffs provided, compute our own.
  std::vector<Diff> diffs = diff_main(text1, text2, true);
  if (diffs.size() > 2) {
    diff_cleanupSemantic(diffs);
    diff_cleanupEfficiency(diffs);
  }

  return patch_make(text1, diffs);
}


std::vector<Patch> diff_match_patch::patch_make(const std::vector<Diff> &diffs) {
  // No origin string provided, compute our own.
  const std::string text1 = diff_text1(diffs);
  return patch_make(text1, diffs);
}


std::vector<Patch> diff_match_patch::patch_make(const std::string &text1,
                                                  const std::string &text2,
                                                  const std::vector<Diff> &diffs) {
  // text2 is entirely unused.
  (void)text2;
  return patch_make(text1, diffs);
}


std::vector<Patch> diff_match_patch::patch_make(const std::string &text1,
                                                  const std::vector<Diff> &diffs) {
  std::vector<Patch> patches;
  if (diffs.empty()) {
    return patches;  // Get rid of the null case.
  }
  Patch patch;
  int char_count1 = 0;  // Number of characters into the text1 string.
  int char_count2 = 0;  // Number of characters into the text2 string.
  // Start with text1 (prepatch_text) and apply the diffs until we arrive at
  // text2 (postpatch_text).
  std::string prepatch_text = text1;
  std::string postpatch_text = text1;
  for (const Diff &aDiff : diffs) {
    if (patch.diffs.empty() && aDiff.operation != EQUAL) {
      // A new patch starts here.
      patch.start1 = char_count1;
      patch.start2 = char_count2;
    }

    switch (aDiff.operation) {
      case INSERT:
        patch.diffs.push_back(aDiff);
        patch.length2 += (int)aDiff.text.size();
        postpatch_text = postpatch_text.substr(0, char_count2)
            + aDiff.text + safeMid(postpatch_text, char_count2);
        break;
      case DELETE:
        patch.length1 += (int)aDiff.text.size();
        patch.diffs.push_back(aDiff);
        postpatch_text = postpatch_text.substr(0, char_count2)
            + safeMid(postpatch_text, char_count2 + (int)aDiff.text.size());
        break;
      case EQUAL:
        if ((int)aDiff.text.size() <= 2 * Patch_Margin
            && !patch.diffs.empty() && !(aDiff == diffs.back())) {
          // Small equality inside a patch.
          patch.diffs.push_back(aDiff);
          patch.length1 += (int)aDiff.text.size();
          patch.length2 += (int)aDiff.text.size();
        }

        if ((int)aDiff.text.size() >= 2 * Patch_Margin) {
          // Time for a new patch.
          if (!patch.diffs.empty()) {
            patch_addContext(patch, prepatch_text);
            patches.push_back(patch);
            patch = Patch();
            // Unlike Unidiff, our patch lists have a rolling context.
            prepatch_text = postpatch_text;
            char_count1 = char_count2;
          }
        }
        break;
    }

    // Update the current character count.
    if (aDiff.operation != INSERT) {
      char_count1 += (int)aDiff.text.size();
    }
    if (aDiff.operation != DELETE) {
      char_count2 += (int)aDiff.text.size();
    }
  }
  // Pick up the leftover patch if not empty.
  if (!patch.diffs.empty()) {
    patch_addContext(patch, prepatch_text);
    patches.push_back(patch);
  }

  return patches;
}


std::vector<Patch> diff_match_patch::patch_deepCopy(std::vector<Patch> &patches) {
  std::vector<Patch> patchesCopy;
  for (const Patch &aPatch : patches) {
    Patch patchCopy = Patch();
    for (const Diff &aDiff : aPatch.diffs) {
      Diff diffCopy = Diff(aDiff.operation, aDiff.text);
      patchCopy.diffs.push_back(diffCopy);
    }
    patchCopy.start1 = aPatch.start1;
    patchCopy.start2 = aPatch.start2;
    patchCopy.length1 = aPatch.length1;
    patchCopy.length2 = aPatch.length2;
    patchesCopy.push_back(patchCopy);
  }
  return patchesCopy;
}


std::pair<std::string, std::vector<bool>> diff_match_patch::patch_apply(
    std::vector<Patch> &patches, const std::string &sourceText) {
  std::string text = sourceText;  // Copy to preserve original.
  if (patches.empty()) {
    return std::make_pair(text, std::vector<bool>(0));
  }

  // Deep copy the patches so that no changes are made to originals.
  std::vector<Patch> patchesCopy = patch_deepCopy(patches);

  std::string nullPadding = patch_addPadding(patchesCopy);
  text = nullPadding + text + nullPadding;
  patch_splitMax(patchesCopy);

  int x = 0;
  int delta = 0;
  std::vector<bool> results(patchesCopy.size());
  for (const Patch &aPatch : patchesCopy) {
    int expected_loc = aPatch.start2 + delta;
    std::string text1 = diff_text1(aPatch.diffs);
    int start_loc;
    int end_loc = -1;
    if ((int)text1.size() > Match_MaxBits) {
      // patch_splitMax will only provide an oversized pattern in the case of
      // a monster delete.
      start_loc = match_main(text, text1.substr(0, Match_MaxBits), expected_loc);
      if (start_loc != -1) {
        end_loc = match_main(text, text1.substr(text1.size() - Match_MaxBits),
            expected_loc + (int)text1.size() - Match_MaxBits);
        if (end_loc == -1 || start_loc >= end_loc) {
          // Can't find valid trailing context.  Drop this patch.
          start_loc = -1;
        }
      }
    } else {
      start_loc = match_main(text, text1, expected_loc);
    }
    if (start_loc == -1) {
      // No match found.
      results[x] = false;
      // Subtract the delta for this failed patch from subsequent patches.
      delta -= aPatch.length2 - aPatch.length1;
    } else {
      // Found a match.
      results[x] = true;
      delta = start_loc - expected_loc;
      std::string text2;
      if (end_loc == -1) {
        text2 = safeMid(text, start_loc, (int)text1.size());
      } else {
        text2 = safeMid(text, start_loc, end_loc + Match_MaxBits - start_loc);
      }
      if (text1 == text2) {
        // Perfect match, just shove the replacement text in.
        text = text.substr(0, start_loc) + diff_text2(aPatch.diffs)
            + safeMid(text, start_loc + (int)text1.size());
      } else {
        // Imperfect match.  Run a diff to get a framework of equivalent
        // indices.
        std::vector<Diff> diffs = diff_main(text1, text2, false);
        if ((int)text1.size() > Match_MaxBits
            && diff_levenshtein(diffs) / static_cast<float>(text1.size())
            > Patch_DeleteThreshold) {
          // The end points match, but the content is unacceptably bad.
          results[x] = false;
        } else {
          diff_cleanupSemanticLossless(diffs);
          int index1 = 0;
          for (const Diff &aDiff : aPatch.diffs) {
            if (aDiff.operation != EQUAL) {
              int index2 = diff_xIndex(diffs, index1);
              if (aDiff.operation == INSERT) {
                // Insertion
                text = text.substr(0, start_loc + index2) + aDiff.text
                    + safeMid(text, start_loc + index2);
              } else if (aDiff.operation == DELETE) {
                // Deletion
                text = text.substr(0, start_loc + index2)
                    + safeMid(text, start_loc + diff_xIndex(diffs,
                    index1 + (int)aDiff.text.size()));
              }
            }
            if (aDiff.operation != DELETE) {
              index1 += (int)aDiff.text.size();
            }
          }
        }
      }
    }
    x++;
  }
  // Strip the padding off.
  text = safeMid(text, (int)nullPadding.size(), (int)text.size()
      - 2 * (int)nullPadding.size());
  return std::make_pair(text, results);
}


std::string diff_match_patch::patch_addPadding(std::vector<Patch> &patches) {
  short paddingLength = Patch_Margin;
  std::string nullPadding;
  for (short x = 1; x <= paddingLength; x++) {
    nullPadding += (char)x;
  }

  // Bump all the patches forward.
  for (Patch &aPatch : patches) {
    aPatch.start1 += paddingLength;
    aPatch.start2 += paddingLength;
  }

  // Add some padding on start of first diff.
  Patch &firstPatch = patches.front();
  std::vector<Diff> &firstPatchDiffs = firstPatch.diffs;
  if (firstPatchDiffs.empty() || firstPatchDiffs.front().operation != EQUAL) {
    // Add nullPadding equality.
    firstPatchDiffs.insert(firstPatchDiffs.begin(), Diff(EQUAL, nullPadding));
    firstPatch.start1 -= paddingLength;  // Should be 0.
    firstPatch.start2 -= paddingLength;  // Should be 0.
    firstPatch.length1 += paddingLength;
    firstPatch.length2 += paddingLength;
  } else if (paddingLength > (int)firstPatchDiffs.front().text.size()) {
    // Grow first equality.
    Diff &firstDiff = firstPatchDiffs.front();
    int extraLength = paddingLength - (int)firstDiff.text.size();
    firstDiff.text = nullPadding.substr((int)firstDiff.text.size(),
        paddingLength - (int)firstDiff.text.size()) + firstDiff.text;
    firstPatch.start1 -= extraLength;
    firstPatch.start2 -= extraLength;
    firstPatch.length1 += extraLength;
    firstPatch.length2 += extraLength;
  }

  // Add some padding on end of last diff.
  Patch &lastPatch = patches.back();
  std::vector<Diff> &lastPatchDiffs = lastPatch.diffs;
  if (lastPatchDiffs.empty() || lastPatchDiffs.back().operation != EQUAL) {
    // Add nullPadding equality.
    lastPatchDiffs.push_back(Diff(EQUAL, nullPadding));
    lastPatch.length1 += paddingLength;
    lastPatch.length2 += paddingLength;
  } else if (paddingLength > (int)lastPatchDiffs.back().text.size()) {
    // Grow last equality.
    Diff &lastDiff = lastPatchDiffs.back();
    int extraLength = paddingLength - (int)lastDiff.text.size();
    lastDiff.text += nullPadding.substr(0, extraLength);
    lastPatch.length1 += extraLength;
    lastPatch.length2 += extraLength;
  }

  return nullPadding;
}


void diff_match_patch::patch_splitMax(std::vector<Patch> &patches) {
  short patch_size = Match_MaxBits;
  std::string precontext, postcontext;
  int start1, start2;
  bool empty;
  Operation diff_type;
  std::string diff_text;

  int pointer = 0;
  while (pointer < (int)patches.size()) {
    if (patches[pointer].length1 <= patch_size) {
      pointer++;
      continue;
    }
    // Copy the big patch before erasing from the vector (erase invalidates references).
    Patch bigpatch = patches[pointer];
    // Remove the big old patch.
    patches.erase(patches.begin() + pointer);
    start1 = bigpatch.start1;
    start2 = bigpatch.start2;
    precontext = "";
    while (!bigpatch.diffs.empty()) {
      // Create one of several smaller patches.
      Patch patch;
      empty = true;
      patch.start1 = start1 - (int)precontext.size();
      patch.start2 = start2 - (int)precontext.size();
      if (!precontext.empty()) {
        patch.length1 = patch.length2 = (int)precontext.size();
        patch.diffs.push_back(Diff(EQUAL, precontext));
      }
      while (!bigpatch.diffs.empty()
          && patch.length1 < patch_size - Patch_Margin) {
        diff_type = bigpatch.diffs.front().operation;
        diff_text = bigpatch.diffs.front().text;
        if (diff_type == INSERT) {
          // Insertions are harmless.
          patch.length2 += (int)diff_text.size();
          start2 += (int)diff_text.size();
          patch.diffs.push_back(bigpatch.diffs.front());
          bigpatch.diffs.erase(bigpatch.diffs.begin());
          empty = false;
        } else if (diff_type == DELETE && patch.diffs.size() == 1
            && patch.diffs.front().operation == EQUAL
            && (int)diff_text.size() > 2 * patch_size) {
          // This is a large deletion.  Let it pass in one chunk.
          patch.length1 += (int)diff_text.size();
          start1 += (int)diff_text.size();
          empty = false;
          patch.diffs.push_back(Diff(diff_type, diff_text));
          bigpatch.diffs.erase(bigpatch.diffs.begin());
        } else {
          // Deletion or equality.  Only take as much as we can stomach.
          diff_text = diff_text.substr(0, std::min((int)diff_text.size(),
              patch_size - patch.length1 - Patch_Margin));
          patch.length1 += (int)diff_text.size();
          start1 += (int)diff_text.size();
          if (diff_type == EQUAL) {
            patch.length2 += (int)diff_text.size();
            start2 += (int)diff_text.size();
          } else {
            empty = false;
          }
          patch.diffs.push_back(Diff(diff_type, diff_text));
          if (diff_text == bigpatch.diffs.front().text) {
            bigpatch.diffs.erase(bigpatch.diffs.begin());
          } else {
            bigpatch.diffs.front().text = bigpatch.diffs.front().text.substr((int)diff_text.size());
          }
        }
      }
      // Compute the head context for the next patch.
      precontext = diff_text2(patch.diffs);
      precontext = precontext.substr(std::max(0, (int)precontext.size() - Patch_Margin));
      // Append the end context for this patch.
      std::string text1 = diff_text1(bigpatch.diffs);
      if ((int)text1.size() > Patch_Margin) {
        postcontext = text1.substr(0, Patch_Margin);
      } else {
        postcontext = text1;
      }
      if (!postcontext.empty()) {
        patch.length1 += (int)postcontext.size();
        patch.length2 += (int)postcontext.size();
        if (!patch.diffs.empty()
            && patch.diffs.back().operation == EQUAL) {
          patch.diffs.back().text += postcontext;
        } else {
          patch.diffs.push_back(Diff(EQUAL, postcontext));
        }
      }
      if (!empty) {
        patches.insert(patches.begin() + pointer, patch);
        pointer++;
      }
    }
    // Don't increment pointer here; we've already moved past bigpatch.
  }
}


std::string diff_match_patch::patch_toText(const std::vector<Patch> &patches) {
  std::string text;
  for (const Patch &aPatch : patches) {
    text += aPatch.toString();
  }
  return text;
}


std::vector<Patch> diff_match_patch::patch_fromText(const std::string &textline) {
  std::vector<Patch> patches;
  if (textline.empty()) {
    return patches;
  }
  std::vector<std::string> text = splitString(textline, '\n', true);
  Patch patch;
  std::regex patchHeader("^@@ -(\\d+),?(\\d*) \\+(\\d+),?(\\d*) @@$");
  std::smatch m;
  char sign;
  std::string line;
  int idx = 0;
  while (idx < (int)text.size()) {
    if (!std::regex_match(text[idx], m, patchHeader)) {
      throw std::string("Invalid patch string: ") + text[idx];
    }

    patch = Patch();
    patch.start1 = std::stoi(m[1].str());
    if (m[2].str().empty()) {
      patch.start1--;
      patch.length1 = 1;
    } else if (m[2].str() == "0") {
      patch.length1 = 0;
    } else {
      patch.start1--;
      patch.length1 = std::stoi(m[2].str());
    }

    patch.start2 = std::stoi(m[3].str());
    if (m[4].str().empty()) {
      patch.start2--;
      patch.length2 = 1;
    } else if (m[4].str() == "0") {
      patch.length2 = 0;
    } else {
      patch.start2--;
      patch.length2 = std::stoi(m[4].str());
    }
    idx++;

    while (idx < (int)text.size()) {
      if (text[idx].empty()) {
        idx++;
        continue;
      }
      sign = text[idx][0];
      line = text[idx].substr(1);
      line = replaceAll(line, "+", "%2B");
      line = percentDecode(line);
      if (sign == '-') {
        // Deletion.
        patch.diffs.push_back(Diff(DELETE, line));
      } else if (sign == '+') {
        // Insertion.
        patch.diffs.push_back(Diff(INSERT, line));
      } else if (sign == ' ') {
        // Minor equality.
        patch.diffs.push_back(Diff(EQUAL, line));
      } else if (sign == '@') {
        // Start of next patch.
        break;
      } else {
        // WTF?
        throw std::string("Invalid patch mode '") + sign + std::string("' in: ") + line;
      }
      idx++;
    }

    patches.push_back(patch);
  }
  return patches;
}
