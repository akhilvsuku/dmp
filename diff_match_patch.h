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

#ifndef DIFF_MATCH_PATCH_H
#define DIFF_MATCH_PATCH_H

/*
 * Functions for diff, match and patch.
 * Computes the difference between two texts to create a patch.
 * Applies the patch onto another text, allowing for errors.
 *
 * @author fraser@google.com (Neil Fraser)
 *
 * C++17 std port (converted from Qt port by mikeslemmer@gmail.com):
 *
 * Here is a trivial sample program which works properly when linked with this
 * library:
 *

 #include <string>
 #include <vector>
 #include <utility>
 #include "diff_match_patch.h"
 int main(int argc, char **argv) {
   diff_match_patch dmp;
   std::string str1 = "First string in diff";
   std::string str2 = "Second string in diff";

   std::string strPatch = dmp.patch_toText(dmp.patch_make(str1, str2));
   std::pair<std::string, std::vector<bool>> out
       = dmp.patch_apply(dmp.patch_fromText(strPatch), str1);
   std::string strResult = out.first;

   // here, strResult will equal str2 above.
   return 0;
 }

 */

#include <string>
#include <vector>
#include <map>
#include <utility>
#include <stack>
#include <regex>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <cmath>
#include <climits>
#include <cstring>
#include <ctime>
#include <limits>
#include <sstream>


/**-
* The data structure representing a diff is a vector of Diff objects:
* {Diff(Operation.DELETE, "Hello"), Diff(Operation.INSERT, "Goodbye"),
*  Diff(Operation.EQUAL, " world.")}
* which means: delete "Hello", add "Goodbye" and keep " world."
*/
enum Operation {
  DELETE, INSERT, EQUAL
};


/**
* Class representing one diff operation.
*/
class Diff {
 public:
  Operation operation;
  // One of: INSERT, DELETE or EQUAL.
  std::string text;
  // The text associated with this diff operation.

  /**
   * Constructor.  Initializes the diff with the provided values.
   * @param operation One of INSERT, DELETE or EQUAL.
   * @param text The text being applied.
   */
  Diff(Operation _operation, const std::string &_text);
  Diff();
  inline bool isNull() const;
  std::string toString() const;
  bool operator==(const Diff &d) const;
  bool operator!=(const Diff &d) const;

  static std::string strOperation(Operation op);
};


/**
* Class representing one patch operation.
*/
class Patch {
 public:
  std::vector<Diff> diffs;
  int start1;
  int start2;
  int length1;
  int length2;

  /**
   * Constructor.  Initializes with an empty list of diffs.
   */
  Patch();
  bool isNull() const;
  std::string toString() const;
};


/**
 * Struct returned by diff_linesToChars.
 * chars1 and chars2 are encoded strings (using char32_t values as line indices).
 * lineArray is the list of unique lines.
 */
struct LinesToCharsResult {
  std::u32string chars1;
  std::u32string chars2;
  std::vector<std::string> lineArray;
};


/**
 * Class containing the diff, match and patch methods.
 * Also contains the behaviour settings.
 */
class diff_match_patch {

  friend class diff_match_patch_test;

 public:
  // Defaults.
  // Set these on your diff_match_patch instance to override the defaults.

  // Number of seconds to map a diff before giving up (0 for infinity).
  float Diff_Timeout;
  // Cost of an empty edit operation in terms of edit characters.
  short Diff_EditCost;
  // At what point is no match declared (0.0 = perfection, 1.0 = very loose).
  float Match_Threshold;
  // How far to search for a match (0 = exact location, 1000+ = broad match).
  // A match this many characters away from the expected location will add
  // 1.0 to the score (0.0 is a perfect match).
  int Match_Distance;
  // When deleting a large block of text (over ~64 characters), how close does
  // the contents have to match the expected contents. (0.0 = perfection,
  // 1.0 = very loose).  Note that Match_Threshold controls how closely the
  // end points of a delete need to match.
  float Patch_DeleteThreshold;
  // Chunk size for context length.
  short Patch_Margin;

  // The number of bits in an int.
  short Match_MaxBits;

 private:
  // Define some regex patterns for matching boundaries.
  static std::regex BLANKLINEEND;
  static std::regex BLANKLINESTART;


 public:

  diff_match_patch();

  //  DIFF FUNCTIONS


  /**
   * Find the differences between two texts.
   * Run a faster slightly less optimal diff.
   * This method allows the 'checklines' of diff_main() to be optional.
   * Most of the time checklines is wanted, so default to true.
   * @param text1 Old string to be diffed.
   * @param text2 New string to be diffed.
   * @return Vector of Diff objects.
   */
  std::vector<Diff> diff_main(const std::string &text1, const std::string &text2);

  /**
   * Find the differences between two texts.
   * @param text1 Old string to be diffed.
   * @param text2 New string to be diffed.
   * @param checklines Speedup flag.  If false, then don't run a
   *     line-level diff first to identify the changed areas.
   *     If true, then run a faster slightly less optimal diff.
   * @return Vector of Diff objects.
   */
  std::vector<Diff> diff_main(const std::string &text1, const std::string &text2, bool checklines);

  /**
   * Find the differences between two texts.  Simplifies the problem by
   * stripping any common prefix or suffix off the texts before diffing.
   * @param text1 Old string to be diffed.
   * @param text2 New string to be diffed.
   * @param checklines Speedup flag.
   * @param deadline Time when the diff should be complete by.
   * @return Vector of Diff objects.
   */
 private:
  std::vector<Diff> diff_main(const std::string &text1, const std::string &text2, bool checklines, clock_t deadline);

  /**
   * Find the differences between two texts.  Assumes that the texts do not
   * have any common prefix or suffix.
   */
 private:
  std::vector<Diff> diff_compute(std::string text1, std::string text2, bool checklines, clock_t deadline);

  /**
   * Do a quick line-level diff on both strings, then rediff the parts for
   * greater accuracy.
   */
 private:
  std::vector<Diff> diff_lineMode(std::string text1, std::string text2, clock_t deadline);

  /**
   * Find the 'middle snake' of a diff, split the problem in two
   * and return the recursively constructed diff.
   */
 protected:
  std::vector<Diff> diff_bisect(const std::string &text1, const std::string &text2, clock_t deadline);

  /**
   * Given the location of the 'middle snake', split the diff in two parts
   * and recurse.
   */
 private:
  std::vector<Diff> diff_bisectSplit(const std::string &text1, const std::string &text2, int x, int y, clock_t deadline);

  /**
   * Split two texts into a list of strings.  Reduce the texts to a string of
   * hashes where each Unicode character represents one line.
   * Returns a LinesToCharsResult struct with encoded strings and line array.
   */
 protected:
  LinesToCharsResult diff_linesToChars(const std::string &text1, const std::string &text2);

  /**
   * Split a text into a list of strings.  Reduce the texts to a string of
   * hashes where each char32_t represents one line.
   * @param text String to encode.
   * @param lineArray List of unique strings.
   * @param lineHash Map of strings to indices.
   * @return Encoded string.
   */
 private:
  std::u32string diff_linesToCharsMunge(const std::string &text,
                                        std::vector<std::string> &lineArray,
                                        std::map<std::string, int> &lineHash);

  /**
   * Rehydrate the text in a diff from a string of line hashes to real lines of
   * text.
   * @param diffs Vector of Diff objects.
   * @param lineArray List of unique strings.
   */
 private:
  void diff_charsToLines(std::vector<Diff> &diffs, const std::vector<std::string> &lineArray);

  /**
   * Determine the common prefix of two strings.
   */
 public:
  int diff_commonPrefix(const std::string &text1, const std::string &text2);

  /**
   * Determine the common suffix of two strings.
   */
 public:
  int diff_commonSuffix(const std::string &text1, const std::string &text2);

  /**
   * Determine if the suffix of one string is the prefix of another.
   */
 protected:
  int diff_commonOverlap(const std::string &text1, const std::string &text2);

  /**
   * Do the two texts share a substring which is at least half the length of
   * the longer text?
   * Returns empty vector if no match.
   */
 protected:
  std::vector<std::string> diff_halfMatch(const std::string &text1, const std::string &text2);

  /**
   * Does a substring of shorttext exist within longtext such that the
   * substring is at least half the length of longtext?
   */
 private:
  std::vector<std::string> diff_halfMatchI(const std::string &longtext, const std::string &shorttext, int i);

  /**
   * Reduce the number of edits by eliminating semantically trivial equalities.
   */
 public:
  void diff_cleanupSemantic(std::vector<Diff> &diffs);

  /**
   * Look for single edits surrounded on both sides by equalities
   * which can be shifted sideways to align the edit to a word boundary.
   */
 public:
  void diff_cleanupSemanticLossless(std::vector<Diff> &diffs);

  /**
   * Given two strings, compute a score representing whether the internal
   * boundary falls on logical boundaries.
   */
 private:
  int diff_cleanupSemanticScore(const std::string &one, const std::string &two);

  /**
   * Reduce the number of edits by eliminating operationally trivial equalities.
   */
 public:
  void diff_cleanupEfficiency(std::vector<Diff> &diffs);

  /**
   * Reorder and merge like edit sections.  Merge equalities.
   */
 public:
  void diff_cleanupMerge(std::vector<Diff> &diffs);

  /**
   * loc is a location in text1, compute and return the equivalent location in
   * text2.
   */
 public:
  int diff_xIndex(const std::vector<Diff> &diffs, int loc);

  /**
   * Convert a Diff list into a pretty HTML report.
   */
 public:
  std::string diff_prettyHtml(const std::vector<Diff> &diffs);

  /**
   * Compute and return the source text (all equalities and deletions).
   */
 public:
  std::string diff_text1(const std::vector<Diff> &diffs);

  /**
   * Compute and return the destination text (all equalities and insertions).
   */
 public:
  std::string diff_text2(const std::vector<Diff> &diffs);

  /**
   * Compute the Levenshtein distance; the number of inserted, deleted or
   * substituted characters.
   */
 public:
  int diff_levenshtein(const std::vector<Diff> &diffs);

  /**
   * Crush the diff into an encoded string which describes the operations
   * required to transform text1 into text2.
   */
 public:
  std::string diff_toDelta(const std::vector<Diff> &diffs);

  /**
   * Given the original text1, and an encoded string which describes the
   * operations required to transform text1 into text2, compute the full diff.
   * @throws std::string If invalid input.
   */
 public:
  std::vector<Diff> diff_fromDelta(const std::string &text1, const std::string &delta);


  //  MATCH FUNCTIONS


  /**
   * Locate the best instance of 'pattern' in 'text' near 'loc'.
   * Returns -1 if no match found.
   */
 public:
  int match_main(const std::string &text, const std::string &pattern, int loc);

  /**
   * Locate the best instance of 'pattern' in 'text' near 'loc' using the
   * Bitap algorithm.  Returns -1 if no match found.
   */
 protected:
  int match_bitap(const std::string &text, const std::string &pattern, int loc);

  /**
   * Compute and return the score for a match with e errors and x location.
   */
 private:
  double match_bitapScore(int e, int x, int loc, const std::string &pattern);

  /**
   * Initialise the alphabet for the Bitap algorithm.
   */
 protected:
  std::map<char, int> match_alphabet(const std::string &pattern);


 //  PATCH FUNCTIONS


  /**
   * Increase the context until it is unique,
   * but don't let the pattern expand beyond Match_MaxBits.
   */
 protected:
  void patch_addContext(Patch &patch, const std::string &text);

  /**
   * Compute a list of patches to turn text1 into text2.
   */
 public:
  std::vector<Patch> patch_make(const std::string &text1, const std::string &text2);

  /**
   * Compute a list of patches to turn text1 into text2.
   * text1 will be derived from the provided diffs.
   */
 public:
  std::vector<Patch> patch_make(const std::vector<Diff> &diffs);

  /**
   * Compute a list of patches to turn text1 into text2.
   * text2 is ignored, diffs are the delta between text1 and text2.
   * @deprecated Prefer patch_make(const std::string &text1, const std::vector<Diff> &diffs).
   */
 public:
  std::vector<Patch> patch_make(const std::string &text1, const std::string &text2, const std::vector<Diff> &diffs);

  /**
   * Compute a list of patches to turn text1 into text2.
   * text2 is not provided, diffs are the delta between text1 and text2.
   */
 public:
  std::vector<Patch> patch_make(const std::string &text1, const std::vector<Diff> &diffs);

  /**
   * Given an array of patches, return another array that is identical.
   */
 public:
  std::vector<Patch> patch_deepCopy(std::vector<Patch> &patches);

  /**
   * Merge a set of patches onto the text.  Return a patched text, as well
   * as a vector of true/false values indicating which patches were applied.
   */
 public:
  std::pair<std::string, std::vector<bool>> patch_apply(std::vector<Patch> &patches, const std::string &text);

  /**
   * Add some padding on text start and end so that edges can match something.
   */
 public:
  std::string patch_addPadding(std::vector<Patch> &patches);

  /**
   * Look through the patches and break up any which are longer than the
   * maximum limit of the match algorithm.
   */
 public:
  void patch_splitMax(std::vector<Patch> &patches);

  /**
   * Take a list of patches and return a textual representation.
   */
 public:
  std::string patch_toText(const std::vector<Patch> &patches);

  /**
   * Parse a textual representation of patches and return a vector of Patch objects.
   * @throws std::string If invalid input.
   */
 public:
  std::vector<Patch> patch_fromText(const std::string &textline);

  /**
   * A safer version of std::string::substr(pos).
   * Returns "" instead of throwing when pos equals string length.
   */
 private:
  static inline std::string safeMid(const std::string &str, int pos) {
    return ((std::string::size_type)pos >= str.size()) ? std::string("") : str.substr(pos);
  }

  /**
   * A safer version of std::string::substr(pos, len).
   * Returns "" instead of throwing when pos equals string length.
   */
 private:
  static inline std::string safeMid(const std::string &str, int pos, int len) {
    if ((std::string::size_type)pos >= str.size()) return std::string("");
    return str.substr(pos, len);
  }

 private:
  // String utility helpers
  static std::string percentEncode(const std::string &text, const std::string &safe);
  static std::string percentDecode(const std::string &text);
  static std::string replaceAll(std::string str, const std::string &from, const std::string &to);
  static std::vector<std::string> splitString(const std::string &str, char delim, bool skipEmpty = false);
  static bool strStartsWith(const std::string &str, const std::string &prefix);
  static bool strEndsWith(const std::string &str, const std::string &suffix);

  // Internal overload of diff_main that operates on u32string (for line-mode)
  std::vector<Diff> diff_main_u32(const std::u32string &text1, const std::u32string &text2, bool checklines, clock_t deadline, int depth = 0);
  std::vector<Diff> diff_compute_u32(std::u32string text1, std::u32string text2, bool checklines, clock_t deadline, int depth = 0);
  std::vector<Diff> diff_bisect_u32(const std::u32string &text1, const std::u32string &text2, clock_t deadline, int depth = 0);
  std::vector<Diff> diff_bisectSplit_u32(const std::u32string &text1, const std::u32string &text2, int x, int y, clock_t deadline, int depth = 0);
  void diff_charsToLines_u32(std::vector<Diff> &diffs, const std::vector<std::string> &lineArray);
};

#endif // DIFF_MATCH_PATCH_H
