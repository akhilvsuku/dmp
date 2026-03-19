/*
 * Diff Match and Patch -- Test Harness
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

#ifndef DIFF_MATCH_PATCH_TEST_H
#define DIFF_MATCH_PATCH_TEST_H

#include <string>
#include <vector>
#include <map>
#include "diff_match_patch.h"

// Sentinel string used as default argument in diffList()
// A Diff(INSERT, DIFFLIST_SENTINEL) signals "end of list".
static const std::string DIFFLIST_SENTINEL = "\x01\x02SENTINEL\x02\x01";

class diff_match_patch_test {
 public:
  diff_match_patch_test();
  void run_all_tests();

  //  DIFF TEST FUNCTIONS
  void testDiffCommonPrefix();
  void testDiffCommonSuffix();
  void testDiffCommonOverlap();
  void testDiffHalfmatch();
  void testDiffLinesToChars();
  void testDiffCharsToLines();
  void testDiffCleanupMerge();
  void testDiffCleanupSemanticLossless();
  void testDiffCleanupSemantic();
  void testDiffCleanupEfficiency();
  void testDiffPrettyHtml();
  void testDiffText();
  void testDiffDelta();
  void testDiffXIndex();
  void testDiffLevenshtein();
  void testDiffBisect();
  void testDiffMain();

  //  MATCH TEST FUNCTIONS
  void testMatchAlphabet();
  void testMatchBitap();
  void testMatchMain();

  //  PATCH TEST FUNCTIONS
  void testPatchObj();
  void testPatchFromText();
  void testPatchToText();
  void testPatchAddContext();
  void testPatchMake();
  void testPatchSplitMax();
  void testPatchAddPadding();
  void testPatchApply();

 private:
  diff_match_patch dmp;

  // Define equality.
  void assertEquals(const std::string &strCase, int n1, int n2);
  void assertEquals(const std::string &strCase, const std::string &s1, const std::string &s2);
  void assertEquals(const std::string &strCase, const Diff &d1, const Diff &d2);
  void assertEquals(const std::string &strCase, const std::vector<Diff> &list1, const std::vector<Diff> &list2);
  void assertEquals(const std::string &strCase, const std::map<char, int> &m1, const std::map<char, int> &m2);
  void assertEquals(const std::string &strCase, const std::vector<std::string> &list1, const std::vector<std::string> &list2);
  void assertEquals(const std::string &strCase, const LinesToCharsResult &r1, const LinesToCharsResult &r2);
  void assertTrue(const std::string &strCase, bool value);
  void assertFalse(const std::string &strCase, bool value);
  void assertEmpty(const std::string &strCase, const std::vector<std::string> &list);

  // Construct the two texts which made up the diff originally.
  std::vector<std::string> diff_rebuildtexts(std::vector<Diff> diffs);
  // Private function for quickly building lists of diffs.
  std::vector<Diff> diffList(
      // Diff(INSERT, DIFFLIST_SENTINEL) is invalid and thus is used as the default argument.
      Diff d1 = Diff(INSERT, DIFFLIST_SENTINEL),
      Diff d2 = Diff(INSERT, DIFFLIST_SENTINEL),
      Diff d3 = Diff(INSERT, DIFFLIST_SENTINEL),
      Diff d4 = Diff(INSERT, DIFFLIST_SENTINEL),
      Diff d5 = Diff(INSERT, DIFFLIST_SENTINEL),
      Diff d6 = Diff(INSERT, DIFFLIST_SENTINEL),
      Diff d7 = Diff(INSERT, DIFFLIST_SENTINEL),
      Diff d8 = Diff(INSERT, DIFFLIST_SENTINEL),
      Diff d9 = Diff(INSERT, DIFFLIST_SENTINEL),
      Diff d10 = Diff(INSERT, DIFFLIST_SENTINEL));
};

#endif // DIFF_MATCH_PATCH_TEST_H
