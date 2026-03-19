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

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <ctime>
#include <limits>
#include <cstring>
#include "diff_match_patch.h"
#include "diff_match_patch_test.h"

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  diff_match_patch_test dmp_test;
  std::cout << "Starting diff_match_patch unit tests." << std::endl;
  dmp_test.run_all_tests();
  std::cout << "Done." << std::endl;
  return 0;
}


diff_match_patch_test::diff_match_patch_test() {
}

void diff_match_patch_test::run_all_tests() {
  auto start = std::chrono::steady_clock::now();
  try {
    testDiffCommonPrefix();
    testDiffCommonSuffix();
    testDiffCommonOverlap();
    testDiffHalfmatch();
    testDiffLinesToChars();
    testDiffCharsToLines();
    testDiffCleanupMerge();
    testDiffCleanupSemanticLossless();
    testDiffCleanupSemantic();
    testDiffCleanupEfficiency();
    testDiffPrettyHtml();
    testDiffText();
    testDiffDelta();
    testDiffXIndex();
    testDiffLevenshtein();
    testDiffBisect();
    testDiffMain();

    testMatchAlphabet();
    testMatchBitap();
    testMatchMain();

    testPatchObj();
    testPatchFromText();
    testPatchToText();
    testPatchAddContext();
    testPatchMake();
    testPatchSplitMax();
    testPatchAddPadding();
    testPatchApply();
    std::cout << "All tests passed." << std::endl;
  } catch (std::string strCase) {
    std::cout << "Test failed: " << strCase << std::endl;
  }
  auto end = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  std::cout << "Total time: " << ms << " ms" << std::endl;
}

//  DIFF TEST FUNCTIONS

void diff_match_patch_test::testDiffCommonPrefix() {
  // Detect any common prefix.
  assertEquals("diff_commonPrefix: Null case.", 0, dmp.diff_commonPrefix("abc", "xyz"));

  assertEquals("diff_commonPrefix: Non-null case.", 4, dmp.diff_commonPrefix("1234abcdef", "1234xyz"));

  assertEquals("diff_commonPrefix: Whole case.", 4, dmp.diff_commonPrefix("1234", "1234xyz"));
}

void diff_match_patch_test::testDiffCommonSuffix() {
  // Detect any common suffix.
  assertEquals("diff_commonSuffix: Null case.", 0, dmp.diff_commonSuffix("abc", "xyz"));

  assertEquals("diff_commonSuffix: Non-null case.", 4, dmp.diff_commonSuffix("abcdef1234", "xyz1234"));

  assertEquals("diff_commonSuffix: Whole case.", 4, dmp.diff_commonSuffix("1234", "xyz1234"));
}

void diff_match_patch_test::testDiffCommonOverlap() {
  // Detect any suffix/prefix overlap.
  assertEquals("diff_commonOverlap: Null case.", 0, dmp.diff_commonOverlap("", "abcd"));

  assertEquals("diff_commonOverlap: Whole case.", 3, dmp.diff_commonOverlap("abc", "abcd"));

  assertEquals("diff_commonOverlap: No overlap.", 0, dmp.diff_commonOverlap("123456", "abcd"));

  assertEquals("diff_commonOverlap: Overlap.", 3, dmp.diff_commonOverlap("123456xxx", "xxxabcd"));

  // Some overly clever languages (C#) may treat ligatures as equal to their
  // component letters.  E.g. U+FB01 == 'fi'
  // In UTF-8, U+FB01 is 0xEF 0xAC 0x81 (3 bytes), 'i' is 0x69
  // They should NOT be equal as byte sequences.
  // The string "\xef\xac\x81i" is U+FB01 followed by 'i' in UTF-8.
  assertEquals("diff_commonOverlap: Unicode.", 0, dmp.diff_commonOverlap("fi", std::string("\xef\xac\x81") + "i"));
}

void diff_match_patch_test::testDiffHalfmatch() {
  // Detect a halfmatch.
  dmp.Diff_Timeout = 1;

  // Helper lambda to split a string by comma
  auto splitComma = [](const std::string &s) {
    std::vector<std::string> result;
    std::string tok;
    for (char c : s) {
      if (c == ',') { result.push_back(tok); tok.clear(); }
      else tok += c;
    }
    result.push_back(tok);
    return result;
  };

  assertEmpty("diff_halfMatch: No match #1.", dmp.diff_halfMatch("1234567890", "abcdef"));

  assertEmpty("diff_halfMatch: No match #2.", dmp.diff_halfMatch("12345", "23"));

  assertEquals("diff_halfMatch: Single Match #1.", splitComma("12,90,a,z,345678"), dmp.diff_halfMatch("1234567890", "a345678z"));

  assertEquals("diff_halfMatch: Single Match #2.", splitComma("a,z,12,90,345678"), dmp.diff_halfMatch("a345678z", "1234567890"));

  assertEquals("diff_halfMatch: Single Match #3.", splitComma("abc,z,1234,0,56789"), dmp.diff_halfMatch("abc56789z", "1234567890"));

  assertEquals("diff_halfMatch: Single Match #4.", splitComma("a,xyz,1,7890,23456"), dmp.diff_halfMatch("a23456xyz", "1234567890"));

  assertEquals("diff_halfMatch: Multiple Matches #1.", splitComma("12123,123121,a,z,1234123451234"), dmp.diff_halfMatch("121231234123451234123121", "a1234123451234z"));

  assertEquals("diff_halfMatch: Multiple Matches #2.", splitComma(",-=-=-=-=-=,x,,x-=-=-=-=-=-=-="), dmp.diff_halfMatch("x-=-=-=-=-=-=-=-=-=-=-=-=", "xx-=-=-=-=-=-=-="));

  assertEquals("diff_halfMatch: Multiple Matches #3.", splitComma("-=-=-=-=-=,,,y,-=-=-=-=-=-=-=y"), dmp.diff_halfMatch("-=-=-=-=-=-=-=-=-=-=-=-=y", "-=-=-=-=-=-=-=yy"));

  // Optimal diff would be -q+x=H-i+e=lloHe+Hu=llo-Hew+y not -qHillo+x=HelloHe-w+Hulloy
  assertEquals("diff_halfMatch: Non-optimal halfmatch.", splitComma("qHillo,w,x,Hulloy,HelloHe"), dmp.diff_halfMatch("qHilloHelloHew", "xHelloHeHulloy"));

  dmp.Diff_Timeout = 0;
  assertEmpty("diff_halfMatch: Optimal no halfmatch.", dmp.diff_halfMatch("qHilloHelloHew", "xHelloHeHulloy"));
}

void diff_match_patch_test::testDiffLinesToChars() {
  // Convert lines down to characters.
  // Use LinesToCharsResult for comparison

  {
    LinesToCharsResult expected;
    expected.lineArray = {"", "alpha\n", "beta\n"};
    // chars1 = indices 1, 2, 1 (alpha, beta, alpha)
    expected.chars1 = std::u32string({1, 2, 1});
    // chars2 = indices 2, 1, 2 (beta, alpha, beta)
    expected.chars2 = std::u32string({2, 1, 2});
    assertEquals("diff_linesToChars:", expected, dmp.diff_linesToChars("alpha\nbeta\nalpha\n", "beta\nalpha\nbeta\n"));
  }

  {
    LinesToCharsResult expected;
    expected.lineArray = {"", "alpha\r\n", "beta\r\n", "\r\n"};
    expected.chars1 = std::u32string();  // empty
    expected.chars2 = std::u32string({1, 2, 3, 3});
    assertEquals("diff_linesToChars:", expected, dmp.diff_linesToChars("", "alpha\r\nbeta\r\n\r\n\r\n"));
  }

  {
    LinesToCharsResult expected;
    expected.lineArray = {"", "a", "b"};
    expected.chars1 = std::u32string({1});
    expected.chars2 = std::u32string({2});
    assertEquals("diff_linesToChars:", expected, dmp.diff_linesToChars("a", "b"));
  }

  // More than 256 to reveal any 8-bit limitations.
  int n = 300;
  {
    std::vector<std::string> lineArray;
    lineArray.push_back("");
    std::string lines;
    std::u32string chars;
    for (int x = 1; x < n + 1; x++) {
      lineArray.push_back(std::to_string(x) + "\n");
      lines += std::to_string(x) + "\n";
      chars += (char32_t)x;
    }
    assertEquals("diff_linesToChars: More than 256 (setup).", n, (int)lineArray.size() - 1);
    assertEquals("diff_linesToChars: More than 256 (setup).", n, (int)chars.size());

    LinesToCharsResult expected;
    expected.lineArray = lineArray;
    expected.chars1 = chars;
    expected.chars2 = std::u32string();
    assertEquals("diff_linesToChars: More than 256.", expected, dmp.diff_linesToChars(lines, ""));
  }
}

void diff_match_patch_test::testDiffCharsToLines() {
  // First check that Diff equality works.
  assertTrue("diff_charsToLines:", Diff(EQUAL, "a") == Diff(EQUAL, "a"));

  assertEquals("diff_charsToLines:", Diff(EQUAL, "a"), Diff(EQUAL, "a"));

  // Convert chars up to lines.
  // We need to call diff_linesToChars and then simulate charsToLines.
  // Since diff_charsToLines is private (via friend), we test it indirectly
  // through diff_lineMode (via diff_main with checklines=true on long text).
  // But the test used to call it directly. Since it's a friend, we can call it.
  // However diff_charsToLines now operates on the internal encoding.
  // We'll test the round-trip through diff_linesToChars + charsToLines via lineMode.

  // Test the >256 case via diff_main with line mode.
  int n = 300;
  std::vector<std::string> lineArray;
  lineArray.push_back("");
  std::string lines;
  std::u32string chars;
  for (int x = 1; x < n + 1; x++) {
    lineArray.push_back(std::to_string(x) + "\n");
    lines += std::to_string(x) + "\n";
    chars += (char32_t)x;
  }
  assertEquals("diff_linesToChars: More than 256 (setup).", n, (int)lineArray.size() - 1);
  assertEquals("diff_linesToChars: More than 256 (setup).", n, (int)chars.size());

  // Build a diffs list with the encoded chars as text, then decode.
  // Encode chars as the 4-byte representation used internally.
  std::string encoded;
  for (char32_t c : chars) {
    encoded += (char)((c >> 24) & 0xFF);
    encoded += (char)((c >> 16) & 0xFF);
    encoded += (char)((c >> 8) & 0xFF);
    encoded += (char)(c & 0xFF);
  }
  std::vector<Diff> diffs;
  diffs.push_back(Diff(DELETE, encoded));
  dmp.diff_charsToLines_u32(diffs, lineArray);
  assertEquals("diff_charsToLines: More than 256.", diffList(Diff(DELETE, lines)), diffs);
}

void diff_match_patch_test::testDiffCleanupMerge() {
  // Cleanup a messy diff.
  std::vector<Diff> diffs;
  dmp.diff_cleanupMerge(diffs);
  assertEquals("diff_cleanupMerge: Null case.", diffList(), diffs);

  diffs = diffList(Diff(EQUAL, "a"), Diff(DELETE, "b"), Diff(INSERT, "c"));
  dmp.diff_cleanupMerge(diffs);
  assertEquals("diff_cleanupMerge: No change case.", diffList(Diff(EQUAL, "a"), Diff(DELETE, "b"), Diff(INSERT, "c")), diffs);

  diffs = diffList(Diff(EQUAL, "a"), Diff(EQUAL, "b"), Diff(EQUAL, "c"));
  dmp.diff_cleanupMerge(diffs);
  assertEquals("diff_cleanupMerge: Merge equalities.", diffList(Diff(EQUAL, "abc")), diffs);

  diffs = diffList(Diff(DELETE, "a"), Diff(DELETE, "b"), Diff(DELETE, "c"));
  dmp.diff_cleanupMerge(diffs);
  assertEquals("diff_cleanupMerge: Merge deletions.", diffList(Diff(DELETE, "abc")), diffs);

  diffs = diffList(Diff(INSERT, "a"), Diff(INSERT, "b"), Diff(INSERT, "c"));
  dmp.diff_cleanupMerge(diffs);
  assertEquals("diff_cleanupMerge: Merge insertions.", diffList(Diff(INSERT, "abc")), diffs);

  diffs = diffList(Diff(DELETE, "a"), Diff(INSERT, "b"), Diff(DELETE, "c"), Diff(INSERT, "d"), Diff(EQUAL, "e"), Diff(EQUAL, "f"));
  dmp.diff_cleanupMerge(diffs);
  assertEquals("diff_cleanupMerge: Merge interweave.", diffList(Diff(DELETE, "ac"), Diff(INSERT, "bd"), Diff(EQUAL, "ef")), diffs);

  diffs = diffList(Diff(DELETE, "a"), Diff(INSERT, "abc"), Diff(DELETE, "dc"));
  dmp.diff_cleanupMerge(diffs);
  assertEquals("diff_cleanupMerge: Prefix and suffix detection.", diffList(Diff(EQUAL, "a"), Diff(DELETE, "d"), Diff(INSERT, "b"), Diff(EQUAL, "c")), diffs);

  diffs = diffList(Diff(EQUAL, "x"), Diff(DELETE, "a"), Diff(INSERT, "abc"), Diff(DELETE, "dc"), Diff(EQUAL, "y"));
  dmp.diff_cleanupMerge(diffs);
  assertEquals("diff_cleanupMerge: Prefix and suffix detection with equalities.", diffList(Diff(EQUAL, "xa"), Diff(DELETE, "d"), Diff(INSERT, "b"), Diff(EQUAL, "cy")), diffs);

  diffs = diffList(Diff(EQUAL, "a"), Diff(INSERT, "ba"), Diff(EQUAL, "c"));
  dmp.diff_cleanupMerge(diffs);
  assertEquals("diff_cleanupMerge: Slide edit left.", diffList(Diff(INSERT, "ab"), Diff(EQUAL, "ac")), diffs);

  diffs = diffList(Diff(EQUAL, "c"), Diff(INSERT, "ab"), Diff(EQUAL, "a"));
  dmp.diff_cleanupMerge(diffs);
  assertEquals("diff_cleanupMerge: Slide edit right.", diffList(Diff(EQUAL, "ca"), Diff(INSERT, "ba")), diffs);

  diffs = diffList(Diff(EQUAL, "a"), Diff(DELETE, "b"), Diff(EQUAL, "c"), Diff(DELETE, "ac"), Diff(EQUAL, "x"));
  dmp.diff_cleanupMerge(diffs);
  assertEquals("diff_cleanupMerge: Slide edit left recursive.", diffList(Diff(DELETE, "abc"), Diff(EQUAL, "acx")), diffs);

  diffs = diffList(Diff(EQUAL, "x"), Diff(DELETE, "ca"), Diff(EQUAL, "c"), Diff(DELETE, "b"), Diff(EQUAL, "a"));
  dmp.diff_cleanupMerge(diffs);
  assertEquals("diff_cleanupMerge: Slide edit right recursive.", diffList(Diff(EQUAL, "xca"), Diff(DELETE, "cba")), diffs);
}

void diff_match_patch_test::testDiffCleanupSemanticLossless() {
  // Slide diffs to match logical boundaries.
  std::vector<Diff> diffs = diffList();
  dmp.diff_cleanupSemanticLossless(diffs);
  assertEquals("diff_cleanupSemantic: Null case.", diffList(), diffs);

  diffs = diffList(Diff(EQUAL, "AAA\r\n\r\nBBB"), Diff(INSERT, "\r\nDDD\r\n\r\nBBB"), Diff(EQUAL, "\r\nEEE"));
  dmp.diff_cleanupSemanticLossless(diffs);
  assertEquals("diff_cleanupSemanticLossless: Blank lines.", diffList(Diff(EQUAL, "AAA\r\n\r\n"), Diff(INSERT, "BBB\r\nDDD\r\n\r\n"), Diff(EQUAL, "BBB\r\nEEE")), diffs);

  diffs = diffList(Diff(EQUAL, "AAA\r\nBBB"), Diff(INSERT, " DDD\r\nBBB"), Diff(EQUAL, " EEE"));
  dmp.diff_cleanupSemanticLossless(diffs);
  assertEquals("diff_cleanupSemanticLossless: Line boundaries.", diffList(Diff(EQUAL, "AAA\r\n"), Diff(INSERT, "BBB DDD\r\n"), Diff(EQUAL, "BBB EEE")), diffs);

  diffs = diffList(Diff(EQUAL, "The c"), Diff(INSERT, "ow and the c"), Diff(EQUAL, "at."));
  dmp.diff_cleanupSemanticLossless(diffs);
  assertEquals("diff_cleanupSemantic: Word boundaries.", diffList(Diff(EQUAL, "The "), Diff(INSERT, "cow and the "), Diff(EQUAL, "cat.")), diffs);

  diffs = diffList(Diff(EQUAL, "The-c"), Diff(INSERT, "ow-and-the-c"), Diff(EQUAL, "at."));
  dmp.diff_cleanupSemanticLossless(diffs);
  assertEquals("diff_cleanupSemantic: Alphanumeric boundaries.", diffList(Diff(EQUAL, "The-"), Diff(INSERT, "cow-and-the-"), Diff(EQUAL, "cat.")), diffs);

  diffs = diffList(Diff(EQUAL, "a"), Diff(DELETE, "a"), Diff(EQUAL, "ax"));
  dmp.diff_cleanupSemanticLossless(diffs);
  assertEquals("diff_cleanupSemantic: Hitting the start.", diffList(Diff(DELETE, "a"), Diff(EQUAL, "aax")), diffs);

  diffs = diffList(Diff(EQUAL, "xa"), Diff(DELETE, "a"), Diff(EQUAL, "a"));
  dmp.diff_cleanupSemanticLossless(diffs);
  assertEquals("diff_cleanupSemantic: Hitting the end.", diffList(Diff(EQUAL, "xaa"), Diff(DELETE, "a")), diffs);

  diffs = diffList(Diff(EQUAL, "The xxx. The "), Diff(INSERT, "zzz. The "), Diff(EQUAL, "yyy."));
  dmp.diff_cleanupSemanticLossless(diffs);
  assertEquals("diff_cleanupSemantic: Sentence boundaries.", diffList(Diff(EQUAL, "The xxx."), Diff(INSERT, " The zzz."), Diff(EQUAL, " The yyy.")), diffs);
}

void diff_match_patch_test::testDiffCleanupSemantic() {
  // Cleanup semantically trivial equalities.
  std::vector<Diff> diffs = diffList();
  dmp.diff_cleanupSemantic(diffs);
  assertEquals("diff_cleanupSemantic: Null case.", diffList(), diffs);

  diffs = diffList(Diff(DELETE, "ab"), Diff(INSERT, "cd"), Diff(EQUAL, "12"), Diff(DELETE, "e"));
  dmp.diff_cleanupSemantic(diffs);
  assertEquals("diff_cleanupSemantic: No elimination #1.", diffList(Diff(DELETE, "ab"), Diff(INSERT, "cd"), Diff(EQUAL, "12"), Diff(DELETE, "e")), diffs);

  diffs = diffList(Diff(DELETE, "abc"), Diff(INSERT, "ABC"), Diff(EQUAL, "1234"), Diff(DELETE, "wxyz"));
  dmp.diff_cleanupSemantic(diffs);
  assertEquals("diff_cleanupSemantic: No elimination #2.", diffList(Diff(DELETE, "abc"), Diff(INSERT, "ABC"), Diff(EQUAL, "1234"), Diff(DELETE, "wxyz")), diffs);

  diffs = diffList(Diff(DELETE, "a"), Diff(EQUAL, "b"), Diff(DELETE, "c"));
  dmp.diff_cleanupSemantic(diffs);
  assertEquals("diff_cleanupSemantic: Simple elimination.", diffList(Diff(DELETE, "abc"), Diff(INSERT, "b")), diffs);

  diffs = diffList(Diff(DELETE, "ab"), Diff(EQUAL, "cd"), Diff(DELETE, "e"), Diff(EQUAL, "f"), Diff(INSERT, "g"));
  dmp.diff_cleanupSemantic(diffs);
  assertEquals("diff_cleanupSemantic: Backpass elimination.", diffList(Diff(DELETE, "abcdef"), Diff(INSERT, "cdfg")), diffs);

  diffs = diffList(Diff(INSERT, "1"), Diff(EQUAL, "A"), Diff(DELETE, "B"), Diff(INSERT, "2"), Diff(EQUAL, "_"), Diff(INSERT, "1"), Diff(EQUAL, "A"), Diff(DELETE, "B"), Diff(INSERT, "2"));
  dmp.diff_cleanupSemantic(diffs);
  assertEquals("diff_cleanupSemantic: Multiple elimination.", diffList(Diff(DELETE, "AB_AB"), Diff(INSERT, "1A2_1A2")), diffs);

  diffs = diffList(Diff(EQUAL, "The c"), Diff(DELETE, "ow and the c"), Diff(EQUAL, "at."));
  dmp.diff_cleanupSemantic(diffs);
  assertEquals("diff_cleanupSemantic: Word boundaries.", diffList(Diff(EQUAL, "The "), Diff(DELETE, "cow and the "), Diff(EQUAL, "cat.")), diffs);

  diffs = diffList(Diff(DELETE, "abcxx"), Diff(INSERT, "xxdef"));
  dmp.diff_cleanupSemantic(diffs);
  assertEquals("diff_cleanupSemantic: No overlap elimination.", diffList(Diff(DELETE, "abcxx"), Diff(INSERT, "xxdef")), diffs);

  diffs = diffList(Diff(DELETE, "abcxxx"), Diff(INSERT, "xxxdef"));
  dmp.diff_cleanupSemantic(diffs);
  assertEquals("diff_cleanupSemantic: Overlap elimination.", diffList(Diff(DELETE, "abc"), Diff(EQUAL, "xxx"), Diff(INSERT, "def")), diffs);

  diffs = diffList(Diff(DELETE, "xxxabc"), Diff(INSERT, "defxxx"));
  dmp.diff_cleanupSemantic(diffs);
  assertEquals("diff_cleanupSemantic: Reverse overlap elimination.", diffList(Diff(INSERT, "def"), Diff(EQUAL, "xxx"), Diff(DELETE, "abc")), diffs);

  diffs = diffList(Diff(DELETE, "abcd1212"), Diff(INSERT, "1212efghi"), Diff(EQUAL, "----"), Diff(DELETE, "A3"), Diff(INSERT, "3BC"));
  dmp.diff_cleanupSemantic(diffs);
  assertEquals("diff_cleanupSemantic: Two overlap eliminations.", diffList(Diff(DELETE, "abcd"), Diff(EQUAL, "1212"), Diff(INSERT, "efghi"), Diff(EQUAL, "----"), Diff(DELETE, "A"), Diff(EQUAL, "3"), Diff(INSERT, "BC")), diffs);
}

void diff_match_patch_test::testDiffCleanupEfficiency() {
  // Cleanup operationally trivial equalities.
  dmp.Diff_EditCost = 4;
  std::vector<Diff> diffs = diffList();
  dmp.diff_cleanupEfficiency(diffs);
  assertEquals("diff_cleanupEfficiency: Null case.", diffList(), diffs);

  diffs = diffList(Diff(DELETE, "ab"), Diff(INSERT, "12"), Diff(EQUAL, "wxyz"), Diff(DELETE, "cd"), Diff(INSERT, "34"));
  dmp.diff_cleanupEfficiency(diffs);
  assertEquals("diff_cleanupEfficiency: No elimination.", diffList(Diff(DELETE, "ab"), Diff(INSERT, "12"), Diff(EQUAL, "wxyz"), Diff(DELETE, "cd"), Diff(INSERT, "34")), diffs);

  diffs = diffList(Diff(DELETE, "ab"), Diff(INSERT, "12"), Diff(EQUAL, "xyz"), Diff(DELETE, "cd"), Diff(INSERT, "34"));
  dmp.diff_cleanupEfficiency(diffs);
  assertEquals("diff_cleanupEfficiency: Four-edit elimination.", diffList(Diff(DELETE, "abxyzcd"), Diff(INSERT, "12xyz34")), diffs);

  diffs = diffList(Diff(INSERT, "12"), Diff(EQUAL, "x"), Diff(DELETE, "cd"), Diff(INSERT, "34"));
  dmp.diff_cleanupEfficiency(diffs);
  assertEquals("diff_cleanupEfficiency: Three-edit elimination.", diffList(Diff(DELETE, "xcd"), Diff(INSERT, "12x34")), diffs);

  diffs = diffList(Diff(DELETE, "ab"), Diff(INSERT, "12"), Diff(EQUAL, "xy"), Diff(INSERT, "34"), Diff(EQUAL, "z"), Diff(DELETE, "cd"), Diff(INSERT, "56"));
  dmp.diff_cleanupEfficiency(diffs);
  assertEquals("diff_cleanupEfficiency: Backpass elimination.", diffList(Diff(DELETE, "abxyzcd"), Diff(INSERT, "12xy34z56")), diffs);

  dmp.Diff_EditCost = 5;
  diffs = diffList(Diff(DELETE, "ab"), Diff(INSERT, "12"), Diff(EQUAL, "wxyz"), Diff(DELETE, "cd"), Diff(INSERT, "34"));
  dmp.diff_cleanupEfficiency(diffs);
  assertEquals("diff_cleanupEfficiency: High cost elimination.", diffList(Diff(DELETE, "abwxyzcd"), Diff(INSERT, "12wxyz34")), diffs);
  dmp.Diff_EditCost = 4;
}

void diff_match_patch_test::testDiffPrettyHtml() {
  // Pretty print.
  std::vector<Diff> diffs = diffList(Diff(EQUAL, "a\n"), Diff(DELETE, "<B>b</B>"), Diff(INSERT, "c&d"));
  assertEquals("diff_prettyHtml:", "<span>a&para;<br></span><del style=\"background:#ffe6e6;\">&lt;B&gt;b&lt;/B&gt;</del><ins style=\"background:#e6ffe6;\">c&amp;d</ins>", dmp.diff_prettyHtml(diffs));
}

void diff_match_patch_test::testDiffText() {
  // Compute the source and destination texts.
  std::vector<Diff> diffs = diffList(Diff(EQUAL, "jump"), Diff(DELETE, "s"), Diff(INSERT, "ed"), Diff(EQUAL, " over "), Diff(DELETE, "the"), Diff(INSERT, "a"), Diff(EQUAL, " lazy"));
  assertEquals("diff_text1:", "jumps over the lazy", dmp.diff_text1(diffs));
  assertEquals("diff_text2:", "jumped over a lazy", dmp.diff_text2(diffs));
}

void diff_match_patch_test::testDiffDelta() {
  // Convert a diff into delta string.
  std::vector<Diff> diffs = diffList(Diff(EQUAL, "jump"), Diff(DELETE, "s"), Diff(INSERT, "ed"), Diff(EQUAL, " over "), Diff(DELETE, "the"), Diff(INSERT, "a"), Diff(EQUAL, " lazy"), Diff(INSERT, "old dog"));
  std::string text1 = dmp.diff_text1(diffs);
  assertEquals("diff_text1: Base text.", "jumps over the lazy", text1);

  std::string delta = dmp.diff_toDelta(diffs);
  assertEquals("diff_toDelta:", "=4\t-1\t+ed\t=6\t-3\t+a\t=5\t+old dog", delta);

  // Convert delta string into a diff.
  assertEquals("diff_fromDelta: Normal.", diffs, dmp.diff_fromDelta(text1, delta));

  // Generates error (19 < 20).
  try {
    dmp.diff_fromDelta(text1 + "x", delta);
    assertFalse("diff_fromDelta: Too long.", true);
  } catch (std::string ex) {
    // Exception expected.
  }

  // Generates error (19 > 18).
  try {
    dmp.diff_fromDelta(text1.substr(1), delta);
    assertFalse("diff_fromDelta: Too short.", true);
  } catch (std::string ex) {
    // Exception expected.
  }

  // Test deltas with special characters.
  // U+0680 in UTF-8: 0xDA 0x80
  // U+0681 in UTF-8: 0xDA 0x81
  // U+0682 in UTF-8: 0xDA 0x82
  // The original test used wchar arrays including a NUL char (\000).
  // We build the strings byte by byte.
  std::string s0680 = "\xda\x80";  // U+0680 in UTF-8
  std::string s0681 = "\xda\x81";  // U+0681 in UTF-8
  std::string s0682 = "\xda\x82";  // U+0682 in UTF-8

  // Original strings from test:
  // L"\u0680 \000 \t %" (7 wchar_t) -> UTF-8: U+0680, ' ', NUL, ' ', TAB, ' ', '%'
  std::string str1 = s0680 + std::string(" \x00 \t %", 6);
  // L"\u0681 \001 \n ^" (7 wchar_t)
  std::string str2 = s0681 + std::string(" \x01 \n ^", 6);
  // L"\u0682 \002 \\ |" (7 wchar_t)
  std::string str3 = s0682 + std::string(" \x02 \\ |", 6);

  diffs = diffList(Diff(EQUAL, str1), Diff(DELETE, str2), Diff(INSERT, str3));
  text1 = dmp.diff_text1(diffs);
  // text1 = str1 + str2
  std::string expected_text1 = str1 + str2;
  assertEquals("diff_text1: Unicode text.", expected_text1, text1);

  delta = dmp.diff_toDelta(diffs);
  // U+0680 is 2 bytes (0xDA 0x80), space, NUL, space, TAB, space, % = 8 bytes total in str1
  // The delta for EQUAL of 8 bytes is "=8"? Wait, str1 is U+0680 (2 bytes) + " \0 \t %" (6 bytes) = 8 bytes
  // str2 is U+0681 (2 bytes) + " \x01 \n ^" (6 bytes) = 8 bytes
  // So text1 = 8+8 = 16 bytes? Wait original test expects =7, -7, so it was QChar (16-bit) based.
  // In the original Qt code, QChar counts as 1 character unit, so str1 had 7 QChars.
  // In UTF-8, each Unicode code point > 0x7F takes multiple bytes.
  // The delta is based on character COUNT (not byte count) in Qt.
  // With std::string and UTF-8, diff_toDelta uses text.size() which is byte count.
  // So the delta values will be different from Qt.
  // We need to re-derive what the expected delta should be with UTF-8 bytes.

  // str1 bytes: DA 80 20 00 20 09 20 25 = 8 bytes
  // str2 bytes: DA 81 20 01 20 0A 20 5E = 8 bytes
  // str3 bytes: DA 82 20 02 20 5C 20 7C = 8 bytes
  // EQUAL(str1=8 bytes): =8
  // DELETE(str2=8 bytes): -8
  // INSERT(str3=8 bytes): percent-encode str3
  // str3: DA->%DA, 82->%82, 20(space)->space, 02->%02, 20->space, 5C(\)->%5C, 20->space, 7C(|)->%7C
  // safe chars include space, so space stays as space
  // Result: +%DA%82 %02 %5C %7C
  assertEquals("diff_toDelta: Unicode.", "=8\t-8\t+%DA%82 %02 %5C %7C", delta);

  assertEquals("diff_fromDelta: Unicode.", diffs, dmp.diff_fromDelta(text1, delta));

  // Verify pool of unchanged characters.
  diffs = diffList(Diff(INSERT, "A-Z a-z 0-9 - _ . ! ~ * \' ( ) ; / ? : @ & = + $ , # "));
  std::string text2 = dmp.diff_text2(diffs);
  assertEquals("diff_text2: Unchanged characters.", "A-Z a-z 0-9 - _ . ! ~ * \' ( ) ; / ? : @ & = + $ , # ", text2);

  delta = dmp.diff_toDelta(diffs);
  assertEquals("diff_toDelta: Unchanged characters.", "+A-Z a-z 0-9 - _ . ! ~ * \' ( ) ; / ? : @ & = + $ , # ", delta);

  // Convert delta string into a diff.
  assertEquals("diff_fromDelta: Unchanged characters.", diffs, dmp.diff_fromDelta("", delta));
}

void diff_match_patch_test::testDiffXIndex() {
  // Translate a location in text1 to text2.
  std::vector<Diff> diffs = diffList(Diff(DELETE, "a"), Diff(INSERT, "1234"), Diff(EQUAL, "xyz"));
  assertEquals("diff_xIndex: Translation on equality.", 5, dmp.diff_xIndex(diffs, 2));

  diffs = diffList(Diff(EQUAL, "a"), Diff(DELETE, "1234"), Diff(EQUAL, "xyz"));
  assertEquals("diff_xIndex: Translation on deletion.", 1, dmp.diff_xIndex(diffs, 3));
}

void diff_match_patch_test::testDiffLevenshtein() {
  std::vector<Diff> diffs = diffList(Diff(DELETE, "abc"), Diff(INSERT, "1234"), Diff(EQUAL, "xyz"));
  assertEquals("diff_levenshtein: Trailing equality.", 4, dmp.diff_levenshtein(diffs));

  diffs = diffList(Diff(EQUAL, "xyz"), Diff(DELETE, "abc"), Diff(INSERT, "1234"));
  assertEquals("diff_levenshtein: Leading equality.", 4, dmp.diff_levenshtein(diffs));

  diffs = diffList(Diff(DELETE, "abc"), Diff(EQUAL, "xyz"), Diff(INSERT, "1234"));
  assertEquals("diff_levenshtein: Middle equality.", 7, dmp.diff_levenshtein(diffs));
}

void diff_match_patch_test::testDiffBisect() {
  // Normal.
  std::string a = "cat";
  std::string b = "map";
  // Since the resulting diff hasn't been normalized, it would be ok if
  // the insertion and deletion pairs are swapped.
  // If the order changes, tweak this test as required.
  std::vector<Diff> diffs = diffList(Diff(DELETE, "c"), Diff(INSERT, "m"), Diff(EQUAL, "a"), Diff(DELETE, "t"), Diff(INSERT, "p"));
  assertEquals("diff_bisect: Normal.", diffs, dmp.diff_bisect(a, b, std::numeric_limits<clock_t>::max()));

  // Timeout.
  diffs = diffList(Diff(DELETE, "cat"), Diff(INSERT, "map"));
  assertEquals("diff_bisect: Timeout.", diffs, dmp.diff_bisect(a, b, 0));
}

void diff_match_patch_test::testDiffMain() {
  // Perform a trivial diff.
  std::vector<Diff> diffs = diffList();
  assertEquals("diff_main: Null case.", diffs, dmp.diff_main("", "", false));

  diffs = diffList(Diff(EQUAL, "abc"));
  assertEquals("diff_main: Equality.", diffs, dmp.diff_main("abc", "abc", false));

  diffs = diffList(Diff(EQUAL, "ab"), Diff(INSERT, "123"), Diff(EQUAL, "c"));
  assertEquals("diff_main: Simple insertion.", diffs, dmp.diff_main("abc", "ab123c", false));

  diffs = diffList(Diff(EQUAL, "a"), Diff(DELETE, "123"), Diff(EQUAL, "bc"));
  assertEquals("diff_main: Simple deletion.", diffs, dmp.diff_main("a123bc", "abc", false));

  diffs = diffList(Diff(EQUAL, "a"), Diff(INSERT, "123"), Diff(EQUAL, "b"), Diff(INSERT, "456"), Diff(EQUAL, "c"));
  assertEquals("diff_main: Two insertions.", diffs, dmp.diff_main("abc", "a123b456c", false));

  diffs = diffList(Diff(EQUAL, "a"), Diff(DELETE, "123"), Diff(EQUAL, "b"), Diff(DELETE, "456"), Diff(EQUAL, "c"));
  assertEquals("diff_main: Two deletions.", diffs, dmp.diff_main("a123b456c", "abc", false));

  // Perform a real diff.
  // Switch off the timeout.
  dmp.Diff_Timeout = 0;
  diffs = diffList(Diff(DELETE, "a"), Diff(INSERT, "b"));
  assertEquals("diff_main: Simple case #1.", diffs, dmp.diff_main("a", "b", false));

  diffs = diffList(Diff(DELETE, "Apple"), Diff(INSERT, "Banana"), Diff(EQUAL, "s are a"), Diff(INSERT, "lso"), Diff(EQUAL, " fruit."));
  assertEquals("diff_main: Simple case #2.", diffs, dmp.diff_main("Apples are a fruit.", "Bananas are also fruit.", false));

  // U+0680 in UTF-8: \xda\x80 ; NUL: \x00 ; TAB: \t
  // U+0680 x TAB -> U+0682 x NUL (using raw UTF-8 bytes)
  {
    std::string s0680 = "\xda\x80";
    std::string s0682 = "\xda\x82";
    std::string input1 = s0680 + std::string("x\t", 2);
    std::string input2 = s0682 + std::string("x\x00", 2);
    // Byte-level diff: \xda is a shared prefix byte; \x80 vs \x82 differ.
    diffs = diffList(Diff(EQUAL, "\xda"), Diff(DELETE, "\x80"), Diff(INSERT, "\x82"), Diff(EQUAL, "x"), Diff(DELETE, "\t"), Diff(INSERT, std::string("\x00", 1)));
    assertEquals("diff_main: Simple case #3.", diffs, dmp.diff_main(input1, input2, false));
  }

  diffs = diffList(Diff(DELETE, "1"), Diff(EQUAL, "a"), Diff(DELETE, "y"), Diff(EQUAL, "b"), Diff(DELETE, "2"), Diff(INSERT, "xab"));
  assertEquals("diff_main: Overlap #1.", diffs, dmp.diff_main("1ayb2", "abxab", false));

  diffs = diffList(Diff(INSERT, "xaxcx"), Diff(EQUAL, "abc"), Diff(DELETE, "y"));
  assertEquals("diff_main: Overlap #2.", diffs, dmp.diff_main("abcy", "xaxcxabc", false));

  diffs = diffList(Diff(DELETE, "ABCD"), Diff(EQUAL, "a"), Diff(DELETE, "="), Diff(INSERT, "-"), Diff(EQUAL, "bcd"), Diff(DELETE, "="), Diff(INSERT, "-"), Diff(EQUAL, "efghijklmnopqrs"), Diff(DELETE, "EFGHIJKLMNOefg"));
  assertEquals("diff_main: Overlap #3.", diffs, dmp.diff_main("ABCDa=bcd=efghijklmnopqrsEFGHIJKLMNOefg", "a-bcd-efghijklmnopqrs", false));

  diffs = diffList(Diff(INSERT, " "), Diff(EQUAL, "a"), Diff(INSERT, "nd"), Diff(EQUAL, " [[Pennsylvania]]"), Diff(DELETE, " and [[New"));
  assertEquals("diff_main: Large equality.", diffs, dmp.diff_main("a [[Pennsylvania]] and [[New", " and [[Pennsylvania]]", false));

  dmp.Diff_Timeout = 0.1f;  // 100ms
  // This test may 'fail' on extremely fast computers.  If so, just increase the text lengths.
  std::string a = "`Twas brillig, and the slithy toves\nDid gyre and gimble in the wabe:\nAll mimsy were the borogoves,\nAnd the mome raths outgrabe.\n";
  std::string b = "I am the very model of a modern major general,\nI've information vegetable, animal, and mineral,\nI know the kings of England, and I quote the fights historical,\nFrom Marathon to Waterloo, in order categorical.\n";
  // Increase the text lengths by 1024 times to ensure a timeout.
  for (int x = 0; x < 10; x++) {
    a = a + a;
    b = b + b;
  }
  clock_t startTime = clock();
  dmp.diff_main(a, b);
  clock_t endTime = clock();
  // Test that we took at least the timeout period.
  assertTrue("diff_main: Timeout min.", dmp.Diff_Timeout * CLOCKS_PER_SEC <= endTime - startTime);
  // Test that we didn't take forever (be forgiving).
  assertTrue("diff_main: Timeout max.", dmp.Diff_Timeout * CLOCKS_PER_SEC * 2 > endTime - startTime);
  dmp.Diff_Timeout = 0;

  // Test the linemode speedup.
  // Must be long to pass the 100 char cutoff.
  a = "1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n";
  b = "abcdefghij\nabcdefghij\nabcdefghij\nabcdefghij\nabcdefghij\nabcdefghij\nabcdefghij\nabcdefghij\nabcdefghij\nabcdefghij\nabcdefghij\nabcdefghij\nabcdefghij\n";
  assertEquals("diff_main: Simple line-mode.", dmp.diff_main(a, b, true), dmp.diff_main(a, b, false));

  a = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
  b = "abcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghij";
  assertEquals("diff_main: Single line-mode.", dmp.diff_main(a, b, true), dmp.diff_main(a, b, false));

  a = "1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n1234567890\n";
  b = "abcdefghij\n1234567890\n1234567890\n1234567890\nabcdefghij\n1234567890\n1234567890\n1234567890\nabcdefghij\n1234567890\n1234567890\n1234567890\nabcdefghij\n";
  std::vector<std::string> texts_linemode = diff_rebuildtexts(dmp.diff_main(a, b, true));
  std::vector<std::string> texts_textmode = diff_rebuildtexts(dmp.diff_main(a, b, false));
  assertEquals("diff_main: Overlap line-mode.", texts_textmode, texts_linemode);

  // Test null inputs -- with std::string, we skip the null-ptr test.
  // std::string cannot represent null, so we omit that test.
}


//  MATCH TEST FUNCTIONS


void diff_match_patch_test::testMatchAlphabet() {
  // Initialise the bitmasks for Bitap.
  std::map<char, int> bitmask;
  bitmask['a'] = 4;
  bitmask['b'] = 2;
  bitmask['c'] = 1;
  assertEquals("match_alphabet: Unique.", bitmask, dmp.match_alphabet("abc"));

  bitmask.clear();
  bitmask['a'] = 37;
  bitmask['b'] = 18;
  bitmask['c'] = 8;
  assertEquals("match_alphabet: Duplicates.", bitmask, dmp.match_alphabet("abcaba"));
}

void diff_match_patch_test::testMatchBitap() {
  // Bitap algorithm.
  dmp.Match_Distance = 100;
  dmp.Match_Threshold = 0.5f;
  assertEquals("match_bitap: Exact match #1.", 5, dmp.match_bitap("abcdefghijk", "fgh", 5));

  assertEquals("match_bitap: Exact match #2.", 5, dmp.match_bitap("abcdefghijk", "fgh", 0));

  assertEquals("match_bitap: Fuzzy match #1.", 4, dmp.match_bitap("abcdefghijk", "efxhi", 0));

  assertEquals("match_bitap: Fuzzy match #2.", 2, dmp.match_bitap("abcdefghijk", "cdefxyhijk", 5));

  assertEquals("match_bitap: Fuzzy match #3.", -1, dmp.match_bitap("abcdefghijk", "bxy", 1));

  assertEquals("match_bitap: Overflow.", 2, dmp.match_bitap("123456789xx0", "3456789x0", 2));

  assertEquals("match_bitap: Before start match.", 0, dmp.match_bitap("abcdef", "xxabc", 4));

  assertEquals("match_bitap: Beyond end match.", 3, dmp.match_bitap("abcdef", "defyy", 4));

  assertEquals("match_bitap: Oversized pattern.", 0, dmp.match_bitap("abcdef", "xabcdefy", 0));

  dmp.Match_Threshold = 0.4f;
  assertEquals("match_bitap: Threshold #1.", 4, dmp.match_bitap("abcdefghijk", "efxyhi", 1));

  dmp.Match_Threshold = 0.3f;
  assertEquals("match_bitap: Threshold #2.", -1, dmp.match_bitap("abcdefghijk", "efxyhi", 1));

  dmp.Match_Threshold = 0.0f;
  assertEquals("match_bitap: Threshold #3.", 1, dmp.match_bitap("abcdefghijk", "bcdef", 1));

  dmp.Match_Threshold = 0.5f;
  assertEquals("match_bitap: Multiple select #1.", 0, dmp.match_bitap("abcdexyzabcde", "abccde", 3));

  assertEquals("match_bitap: Multiple select #2.", 8, dmp.match_bitap("abcdexyzabcde", "abccde", 5));

  dmp.Match_Distance = 10;  // Strict location.
  assertEquals("match_bitap: Distance test #1.", -1, dmp.match_bitap("abcdefghijklmnopqrstuvwxyz", "abcdefg", 24));

  assertEquals("match_bitap: Distance test #2.", 0, dmp.match_bitap("abcdefghijklmnopqrstuvwxyz", "abcdxxefg", 1));

  dmp.Match_Distance = 1000;  // Loose location.
  assertEquals("match_bitap: Distance test #3.", 0, dmp.match_bitap("abcdefghijklmnopqrstuvwxyz", "abcdefg", 24));
}

void diff_match_patch_test::testMatchMain() {
  // Full match.
  assertEquals("match_main: Equality.", 0, dmp.match_main("abcdef", "abcdef", 1000));

  assertEquals("match_main: Null text.", -1, dmp.match_main("", "abcdef", 1));

  assertEquals("match_main: Null pattern.", 3, dmp.match_main("abcdef", "", 3));

  assertEquals("match_main: Exact match.", 3, dmp.match_main("abcdef", "de", 3));

  dmp.Match_Threshold = 0.7f;
  assertEquals("match_main: Complex match.", 4, dmp.match_main("I am the very model of a modern major general.", " that berry ", 5));
  dmp.Match_Threshold = 0.5f;

  // Test null inputs -- omitted for std::string (cannot pass null)
}


//  PATCH TEST FUNCTIONS


void diff_match_patch_test::testPatchObj() {
  // Patch Object.
  Patch p;
  p.start1 = 20;
  p.start2 = 21;
  p.length1 = 18;
  p.length2 = 17;
  p.diffs = diffList(Diff(EQUAL, "jump"), Diff(DELETE, "s"), Diff(INSERT, "ed"), Diff(EQUAL, " over "), Diff(DELETE, "the"), Diff(INSERT, "a"), Diff(EQUAL, "\nlaz"));
  std::string strp = "@@ -21,18 +22,17 @@\n jump\n-s\n+ed\n  over \n-the\n+a\n %0Alaz\n";
  assertEquals("Patch: toString.", strp, p.toString());
}

void diff_match_patch_test::testPatchFromText() {
  assertTrue("patch_fromText: #0.", dmp.patch_fromText("").empty());

  std::string strp = "@@ -21,18 +22,17 @@\n jump\n-s\n+ed\n  over \n-the\n+a\n %0Alaz\n";
  assertEquals("patch_fromText: #1.", strp, dmp.patch_fromText(strp)[0].toString());

  assertEquals("patch_fromText: #2.", "@@ -1 +1 @@\n-a\n+b\n", dmp.patch_fromText("@@ -1 +1 @@\n-a\n+b\n")[0].toString());

  assertEquals("patch_fromText: #3.", "@@ -1,3 +0,0 @@\n-abc\n", dmp.patch_fromText("@@ -1,3 +0,0 @@\n-abc\n")[0].toString());

  assertEquals("patch_fromText: #4.", "@@ -0,0 +1,3 @@\n+abc\n", dmp.patch_fromText("@@ -0,0 +1,3 @@\n+abc\n")[0].toString());

  // Generates error.
  try {
    dmp.patch_fromText("Bad\nPatch\n");
    assertFalse("patch_fromText: #5.", true);
  } catch (std::string ex) {
    // Exception expected.
  }
}

void diff_match_patch_test::testPatchToText() {
  std::string strp = "@@ -21,18 +22,17 @@\n jump\n-s\n+ed\n  over \n-the\n+a\n  laz\n";
  std::vector<Patch> patches;
  patches = dmp.patch_fromText(strp);
  assertEquals("patch_toText: Single", strp, dmp.patch_toText(patches));

  strp = "@@ -1,9 +1,9 @@\n-f\n+F\n oo+fooba\n@@ -7,9 +7,9 @@\n obar\n-,\n+.\n  tes\n";
  patches = dmp.patch_fromText(strp);
  assertEquals("patch_toText: Dual", strp, dmp.patch_toText(patches));
}

void diff_match_patch_test::testPatchAddContext() {
  dmp.Patch_Margin = 4;
  Patch p;
  p = dmp.patch_fromText("@@ -21,4 +21,10 @@\n-jump\n+somersault\n")[0];
  dmp.patch_addContext(p, "The quick brown fox jumps over the lazy dog.");
  assertEquals("patch_addContext: Simple case.", "@@ -17,12 +17,18 @@\n fox \n-jump\n+somersault\n s ov\n", p.toString());

  p = dmp.patch_fromText("@@ -21,4 +21,10 @@\n-jump\n+somersault\n")[0];
  dmp.patch_addContext(p, "The quick brown fox jumps.");
  assertEquals("patch_addContext: Not enough trailing context.", "@@ -17,10 +17,16 @@\n fox \n-jump\n+somersault\n s.\n", p.toString());

  p = dmp.patch_fromText("@@ -3 +3,2 @@\n-e\n+at\n")[0];
  dmp.patch_addContext(p, "The quick brown fox jumps.");
  assertEquals("patch_addContext: Not enough leading context.", "@@ -1,7 +1,8 @@\n Th\n-e\n+at\n  qui\n", p.toString());

  p = dmp.patch_fromText("@@ -3 +3,2 @@\n-e\n+at\n")[0];
  dmp.patch_addContext(p, "The quick brown fox jumps.  The quick brown fox crashes.");
  assertEquals("patch_addContext: Ambiguity.", "@@ -1,27 +1,28 @@\n Th\n-e\n+at\n  quick brown fox jumps. \n", p.toString());
}

void diff_match_patch_test::testPatchMake() {
  std::vector<Patch> patches;
  patches = dmp.patch_make("", "");
  assertEquals("patch_make: Null case", "", dmp.patch_toText(patches));

  std::string text1 = "The quick brown fox jumps over the lazy dog.";
  std::string text2 = "That quick brown fox jumped over a lazy dog.";
  std::string expectedPatch = "@@ -1,8 +1,7 @@\n Th\n-at\n+e\n  qui\n@@ -21,17 +21,18 @@\n jump\n-ed\n+s\n  over \n-a\n+the\n  laz\n";
  // The second patch must be "-21,17 +21,18", not "-22,17 +21,18" due to rolling context.
  patches = dmp.patch_make(text2, text1);
  assertEquals("patch_make: Text2+Text1 inputs", expectedPatch, dmp.patch_toText(patches));

  expectedPatch = "@@ -1,11 +1,12 @@\n Th\n-e\n+at\n  quick b\n@@ -22,18 +22,17 @@\n jump\n-s\n+ed\n  over \n-the\n+a\n  laz\n";
  patches = dmp.patch_make(text1, text2);
  assertEquals("patch_make: Text1+Text2 inputs", expectedPatch, dmp.patch_toText(patches));

  std::vector<Diff> diffs = dmp.diff_main(text1, text2, false);
  patches = dmp.patch_make(diffs);
  assertEquals("patch_make: Diff input", expectedPatch, dmp.patch_toText(patches));

  patches = dmp.patch_make(text1, diffs);
  assertEquals("patch_make: Text1+Diff inputs", expectedPatch, dmp.patch_toText(patches));

  patches = dmp.patch_make(text1, text2, diffs);
  assertEquals("patch_make: Text1+Text2+Diff inputs (deprecated)", expectedPatch, dmp.patch_toText(patches));

  patches = dmp.patch_make("`1234567890-=[]\\;',./", "~!@#$%^&*()_+{}|:\"<>?");
  assertEquals("patch_toText: Character encoding.", "@@ -1,21 +1,21 @@\n-%601234567890-=%5B%5D%5C;',./\n+~!@#$%25%5E&*()_+%7B%7D%7C:%22%3C%3E?\n", dmp.patch_toText(patches));

  diffs = diffList(Diff(DELETE, "`1234567890-=[]\\;',./"), Diff(INSERT, "~!@#$%^&*()_+{}|:\"<>?"));
  assertEquals("patch_fromText: Character decoding.", diffs, dmp.patch_fromText("@@ -1,21 +1,21 @@\n-%601234567890-=%5B%5D%5C;',./\n+~!@#$%25%5E&*()_+%7B%7D%7C:%22%3C%3E?\n")[0].diffs);

  text1 = "";
  for (int x = 0; x < 100; x++) {
    text1 += "abcdef";
  }
  text2 = text1 + "123";
  expectedPatch = "@@ -573,28 +573,31 @@\n cdefabcdefabcdefabcdefabcdef\n+123\n";
  patches = dmp.patch_make(text1, text2);
  assertEquals("patch_make: Long string with repeats.", expectedPatch, dmp.patch_toText(patches));

  // Test null inputs -- omitted for std::string (cannot pass null)
}

void diff_match_patch_test::testPatchSplitMax() {
  // Assumes that Match_MaxBits is 32.
  std::vector<Patch> patches;
  patches = dmp.patch_make("abcdefghijklmnopqrstuvwxyz01234567890", "XabXcdXefXghXijXklXmnXopXqrXstXuvXwxXyzX01X23X45X67X89X0");
  dmp.patch_splitMax(patches);
  assertEquals("patch_splitMax: #1.", "@@ -1,32 +1,46 @@\n+X\n ab\n+X\n cd\n+X\n ef\n+X\n gh\n+X\n ij\n+X\n kl\n+X\n mn\n+X\n op\n+X\n qr\n+X\n st\n+X\n uv\n+X\n wx\n+X\n yz\n+X\n 012345\n@@ -25,13 +39,18 @@\n zX01\n+X\n 23\n+X\n 45\n+X\n 67\n+X\n 89\n+X\n 0\n", dmp.patch_toText(patches));

  patches = dmp.patch_make("abcdef1234567890123456789012345678901234567890123456789012345678901234567890uvwxyz", "abcdefuvwxyz");
  std::string oldToText = dmp.patch_toText(patches);
  dmp.patch_splitMax(patches);
  assertEquals("patch_splitMax: #2.", oldToText, dmp.patch_toText(patches));

  patches = dmp.patch_make("1234567890123456789012345678901234567890123456789012345678901234567890", "abc");
  dmp.patch_splitMax(patches);
  assertEquals("patch_splitMax: #3.", "@@ -1,32 +1,4 @@\n-1234567890123456789012345678\n 9012\n@@ -29,32 +1,4 @@\n-9012345678901234567890123456\n 7890\n@@ -57,14 +1,3 @@\n-78901234567890\n+abc\n", dmp.patch_toText(patches));

  patches = dmp.patch_make("abcdefghij , h : 0 , t : 1 abcdefghij , h : 0 , t : 1 abcdefghij , h : 0 , t : 1", "abcdefghij , h : 1 , t : 1 abcdefghij , h : 1 , t : 1 abcdefghij , h : 0 , t : 1");
  dmp.patch_splitMax(patches);
  assertEquals("patch_splitMax: #4.", "@@ -2,32 +2,32 @@\n bcdefghij , h : \n-0\n+1\n  , t : 1 abcdef\n@@ -29,32 +29,32 @@\n bcdefghij , h : \n-0\n+1\n  , t : 1 abcdef\n", dmp.patch_toText(patches));
}

void diff_match_patch_test::testPatchAddPadding() {
  std::vector<Patch> patches;
  patches = dmp.patch_make("", "test");
  assertEquals("patch_addPadding: Both edges full.", "@@ -0,0 +1,4 @@\n+test\n", dmp.patch_toText(patches));
  dmp.patch_addPadding(patches);
  assertEquals("patch_addPadding: Both edges full.", "@@ -1,8 +1,12 @@\n %01%02%03%04\n+test\n %01%02%03%04\n", dmp.patch_toText(patches));

  patches = dmp.patch_make("XY", "XtestY");
  assertEquals("patch_addPadding: Both edges partial.", "@@ -1,2 +1,6 @@\n X\n+test\n Y\n", dmp.patch_toText(patches));
  dmp.patch_addPadding(patches);
  assertEquals("patch_addPadding: Both edges partial.", "@@ -2,8 +2,12 @@\n %02%03%04X\n+test\n Y%01%02%03\n", dmp.patch_toText(patches));

  patches = dmp.patch_make("XXXXYYYY", "XXXXtestYYYY");
  assertEquals("patch_addPadding: Both edges none.", "@@ -1,8 +1,12 @@\n XXXX\n+test\n YYYY\n", dmp.patch_toText(patches));
  dmp.patch_addPadding(patches);
  assertEquals("patch_addPadding: Both edges none.", "@@ -5,8 +5,12 @@\n XXXX\n+test\n YYYY\n", dmp.patch_toText(patches));
}

void diff_match_patch_test::testPatchApply() {
  dmp.Match_Distance = 1000;
  dmp.Match_Threshold = 0.5f;
  dmp.Patch_DeleteThreshold = 0.5f;
  std::vector<Patch> patches;
  patches = dmp.patch_make("", "");
  std::pair<std::string, std::vector<bool>> results = dmp.patch_apply(patches, "Hello world.");
  std::vector<bool> boolArray = results.second;

  std::string resultStr = results.first + "\t" + std::to_string((int)boolArray.size());
  assertEquals("patch_apply: Null case.", "Hello world.\t0", resultStr);

  patches = dmp.patch_make("The quick brown fox jumps over the lazy dog.", "That quick brown fox jumped over a lazy dog.");
  results = dmp.patch_apply(patches, "The quick brown fox jumps over the lazy dog.");
  boolArray = results.second;
  resultStr = results.first + "\t" + (boolArray[0] ? "true" : "false") + "\t" + (boolArray[1] ? "true" : "false");
  assertEquals("patch_apply: Exact match.", "That quick brown fox jumped over a lazy dog.\ttrue\ttrue", resultStr);

  results = dmp.patch_apply(patches, "The quick red rabbit jumps over the tired tiger.");
  boolArray = results.second;
  resultStr = results.first + "\t" + (boolArray[0] ? "true" : "false") + "\t" + (boolArray[1] ? "true" : "false");
  assertEquals("patch_apply: Partial match.", "That quick red rabbit jumped over a tired tiger.\ttrue\ttrue", resultStr);

  results = dmp.patch_apply(patches, "I am the very model of a modern major general.");
  boolArray = results.second;
  resultStr = results.first + "\t" + (boolArray[0] ? "true" : "false") + "\t" + (boolArray[1] ? "true" : "false");
  assertEquals("patch_apply: Failed match.", "I am the very model of a modern major general.\tfalse\tfalse", resultStr);

  patches = dmp.patch_make("x1234567890123456789012345678901234567890123456789012345678901234567890y", "xabcy");
  results = dmp.patch_apply(patches, "x123456789012345678901234567890-----++++++++++-----123456789012345678901234567890y");
  boolArray = results.second;
  resultStr = results.first + "\t" + (boolArray[0] ? "true" : "false") + "\t" + (boolArray[1] ? "true" : "false");
  assertEquals("patch_apply: Big delete, small change.", "xabcy\ttrue\ttrue", resultStr);

  patches = dmp.patch_make("x1234567890123456789012345678901234567890123456789012345678901234567890y", "xabcy");
  results = dmp.patch_apply(patches, "x12345678901234567890---------------++++++++++---------------12345678901234567890y");
  boolArray = results.second;
  resultStr = results.first + "\t" + (boolArray[0] ? "true" : "false") + "\t" + (boolArray[1] ? "true" : "false");
  assertEquals("patch_apply: Big delete, large change 1.", "xabc12345678901234567890---------------++++++++++---------------12345678901234567890y\tfalse\ttrue", resultStr);

  dmp.Patch_DeleteThreshold = 0.6f;
  patches = dmp.patch_make("x1234567890123456789012345678901234567890123456789012345678901234567890y", "xabcy");
  results = dmp.patch_apply(patches, "x12345678901234567890---------------++++++++++---------------12345678901234567890y");
  boolArray = results.second;
  resultStr = results.first + "\t" + (boolArray[0] ? "true" : "false") + "\t" + (boolArray[1] ? "true" : "false");
  assertEquals("patch_apply: Big delete, large change 2.", "xabcy\ttrue\ttrue", resultStr);
  dmp.Patch_DeleteThreshold = 0.5f;

  dmp.Match_Threshold = 0.0f;
  dmp.Match_Distance = 0;
  patches = dmp.patch_make("abcdefghijklmnopqrstuvwxyz--------------------1234567890", "abcXXXXXXXXXXdefghijklmnopqrstuvwxyz--------------------1234567YYYYYYYYYY890");
  results = dmp.patch_apply(patches, "ABCDEFGHIJKLMNOPQRSTUVWXYZ--------------------1234567890");
  boolArray = results.second;
  resultStr = results.first + "\t" + (boolArray[0] ? "true" : "false") + "\t" + (boolArray[1] ? "true" : "false");
  assertEquals("patch_apply: Compensate for failed patch.", "ABCDEFGHIJKLMNOPQRSTUVWXYZ--------------------1234567YYYYYYYYYY890\tfalse\ttrue", resultStr);
  dmp.Match_Threshold = 0.5f;
  dmp.Match_Distance = 1000;

  patches = dmp.patch_make("", "test");
  std::string patchStr = dmp.patch_toText(patches);
  dmp.patch_apply(patches, "");
  assertEquals("patch_apply: No side effects.", patchStr, dmp.patch_toText(patches));

  patches = dmp.patch_make("The quick brown fox jumps over the lazy dog.", "Woof");
  patchStr = dmp.patch_toText(patches);
  dmp.patch_apply(patches, "The quick brown fox jumps over the lazy dog.");
  assertEquals("patch_apply: No side effects with major delete.", patchStr, dmp.patch_toText(patches));

  patches = dmp.patch_make("", "test");
  results = dmp.patch_apply(patches, "");
  boolArray = results.second;
  resultStr = results.first + "\t" + (boolArray[0] ? "true" : "false");
  assertEquals("patch_apply: Edge exact match.", "test\ttrue", resultStr);

  patches = dmp.patch_make("XY", "XtestY");
  results = dmp.patch_apply(patches, "XY");
  boolArray = results.second;
  resultStr = results.first + "\t" + (boolArray[0] ? "true" : "false");
  assertEquals("patch_apply: Near edge exact match.", "XtestY\ttrue", resultStr);

  patches = dmp.patch_make("y", "y123");
  results = dmp.patch_apply(patches, "x");
  boolArray = results.second;
  resultStr = results.first + "\t" + (boolArray[0] ? "true" : "false");
  assertEquals("patch_apply: Edge partial match.", "x123\ttrue", resultStr);
}


void diff_match_patch_test::assertEquals(const std::string &strCase, int n1, int n2) {
  if (n1 != n2) {
    std::cout << strCase << " FAIL\nExpected: " << n1 << "\nActual: " << n2 << std::endl;
    throw strCase;
  }
  std::cout << strCase << " OK" << std::endl;
}

void diff_match_patch_test::assertEquals(const std::string &strCase, const std::string &s1, const std::string &s2) {
  if (s1 != s2) {
    std::cout << strCase << " FAIL\nExpected: " << s1 << "\nActual: " << s2 << std::endl;
    throw strCase;
  }
  std::cout << strCase << " OK" << std::endl;
}

void diff_match_patch_test::assertEquals(const std::string &strCase, const Diff &d1, const Diff &d2) {
  if (d1 != d2) {
    std::cout << strCase << " FAIL\nExpected: " << d1.toString() << "\nActual: " << d2.toString() << std::endl;
    throw strCase;
  }
  std::cout << strCase << " OK" << std::endl;
}

void diff_match_patch_test::assertEquals(const std::string &strCase, const std::vector<Diff> &list1, const std::vector<Diff> &list2) {
  bool fail = false;
  if (list1.size() == list2.size()) {
    for (std::size_t i = 0; i < list1.size(); i++) {
      if (list1[i] != list2[i]) {
        fail = true;
        break;
      }
    }
  } else {
    fail = true;
  }

  if (fail) {
    // Build human readable description of both lists.
    std::string listString1 = "(";
    bool first = true;
    for (const Diff &d : list1) {
      if (!first) listString1 += ", ";
      listString1 += d.toString();
      first = false;
    }
    listString1 += ")";
    std::string listString2 = "(";
    first = true;
    for (const Diff &d : list2) {
      if (!first) listString2 += ", ";
      listString2 += d.toString();
      first = false;
    }
    listString2 += ")";
    std::cout << strCase << " FAIL\nExpected: " << listString1 << "\nActual: " << listString2 << std::endl;
    throw strCase;
  }
  std::cout << strCase << " OK" << std::endl;
}

void diff_match_patch_test::assertEquals(const std::string &strCase, const LinesToCharsResult &r1, const LinesToCharsResult &r2) {
  bool fail = false;
  if (r1.chars1 != r2.chars1 || r1.chars2 != r2.chars2 || r1.lineArray != r2.lineArray) {
    fail = true;
  }
  if (fail) {
    std::cout << strCase << " FAIL\n";
    std::cout << "  chars1 match: " << (r1.chars1 == r2.chars1 ? "yes" : "no") << "\n";
    std::cout << "  chars2 match: " << (r1.chars2 == r2.chars2 ? "yes" : "no") << "\n";
    std::cout << "  lineArray match: " << (r1.lineArray == r2.lineArray ? "yes" : "no") << std::endl;
    throw strCase;
  }
  std::cout << strCase << " OK" << std::endl;
}

void diff_match_patch_test::assertEquals(const std::string &strCase, const std::map<char, int> &m1, const std::map<char, int> &m2) {
  if (m1 != m2) {
    std::cout << strCase << " FAIL\nMaps differ." << std::endl;
    throw strCase;
  }
  std::cout << strCase << " OK" << std::endl;
}

void diff_match_patch_test::assertEquals(const std::string &strCase, const std::vector<std::string> &list1, const std::vector<std::string> &list2) {
  if (list1 != list2) {
    std::string s1, s2;
    for (const std::string &s : list1) { s1 += s + ","; }
    for (const std::string &s : list2) { s2 += s + ","; }
    std::cout << strCase << " FAIL\nExpected: " << s1 << "\nActual: " << s2 << std::endl;
    throw strCase;
  }
  std::cout << strCase << " OK" << std::endl;
}

void diff_match_patch_test::assertTrue(const std::string &strCase, bool value) {
  if (!value) {
    std::cout << strCase << " FAIL\nExpected: true\nActual: false" << std::endl;
    throw strCase;
  }
  std::cout << strCase << " OK" << std::endl;
}

void diff_match_patch_test::assertFalse(const std::string &strCase, bool value) {
  if (value) {
    std::cout << strCase << " FAIL\nExpected: false\nActual: true" << std::endl;
    throw strCase;
  }
  std::cout << strCase << " OK" << std::endl;
}


// Construct the two texts which made up the diff originally.
std::vector<std::string> diff_match_patch_test::diff_rebuildtexts(std::vector<Diff> diffs) {
  std::vector<std::string> text = {"", ""};
  for (const Diff &myDiff : diffs) {
    if (myDiff.operation != INSERT) {
      text[0] += myDiff.text;
    }
    if (myDiff.operation != DELETE) {
      text[1] += myDiff.text;
    }
  }
  return text;
}

void diff_match_patch_test::assertEmpty(const std::string &strCase, const std::vector<std::string> &list) {
  if (!list.empty()) {
    std::cout << strCase << " FAIL\nExpected empty list." << std::endl;
    throw strCase;
  }
  std::cout << strCase << " OK" << std::endl;
}


// Private function for quickly building lists of diffs.
std::vector<Diff> diff_match_patch_test::diffList(Diff d1, Diff d2, Diff d3, Diff d4, Diff d5,
  Diff d6, Diff d7, Diff d8, Diff d9, Diff d10) {
  // Diff(INSERT, DIFFLIST_SENTINEL) is invalid and thus is used as the default argument.
  std::vector<Diff> listRet;
  if (d1.operation == INSERT && d1.text == DIFFLIST_SENTINEL) {
    return listRet;
  }
  listRet.push_back(d1);

  if (d2.operation == INSERT && d2.text == DIFFLIST_SENTINEL) {
    return listRet;
  }
  listRet.push_back(d2);

  if (d3.operation == INSERT && d3.text == DIFFLIST_SENTINEL) {
    return listRet;
  }
  listRet.push_back(d3);

  if (d4.operation == INSERT && d4.text == DIFFLIST_SENTINEL) {
    return listRet;
  }
  listRet.push_back(d4);

  if (d5.operation == INSERT && d5.text == DIFFLIST_SENTINEL) {
    return listRet;
  }
  listRet.push_back(d5);

  if (d6.operation == INSERT && d6.text == DIFFLIST_SENTINEL) {
    return listRet;
  }
  listRet.push_back(d6);

  if (d7.operation == INSERT && d7.text == DIFFLIST_SENTINEL) {
    return listRet;
  }
  listRet.push_back(d7);

  if (d8.operation == INSERT && d8.text == DIFFLIST_SENTINEL) {
    return listRet;
  }
  listRet.push_back(d8);

  if (d9.operation == INSERT && d9.text == DIFFLIST_SENTINEL) {
    return listRet;
  }
  listRet.push_back(d9);

  if (d10.operation == INSERT && d10.text == DIFFLIST_SENTINEL) {
    return listRet;
  }
  listRet.push_back(d10);

  return listRet;
}
