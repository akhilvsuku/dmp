/*
 * consolefilecompare — compare two text files using diff-match-patch
 *
 * Usage:  consolefilecompare <file1> <file2>
 *
 * Output:
 *   EQUAL   text  — printed in default color
 *   DELETE  text  — printed in red   (text present only in file1)
 *   INSERT  text  — printed in green (text present only in file2)
 */

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#ifndef _WIN32
#  include <unistd.h>
#endif

#include "../diff_match_patch.h"

// ── ANSI color helpers ────────────────────────────────────────────────────────

static const char* RESET  = "\033[0m";
static const char* RED    = "\033[31m";    // DELETE
static const char* GREEN  = "\033[32m";    // INSERT
static const char* BOLD   = "\033[1m";

static bool supportsColor() {
    const char* term = std::getenv("TERM");
    if (term && std::string(term) == "dumb") return false;
#ifdef _WIN32
    // On Windows enable virtual terminal processing if possible
    return false;   // safe fallback; force-enable via /ANSI or Windows Terminal
#else
    return isatty(STDOUT_FILENO);
#endif
}

// ── file reading ──────────────────────────────────────────────────────────────

static bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Error: cannot open file '" << path << "'\n";
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// ── diff rendering ────────────────────────────────────────────────────────────

static void renderDiffs(const std::vector<Diff>& diffs, bool color) {

    // --- summary counters ---
    int insertedChars = 0;
    int deletedChars  = 0;

    if (color) {
        std::cout << "\n" << BOLD
                  << "── diff output ─────────────────────────────────────────"
                  << RESET << "\n\n";
    } else {
        std::cout << "\n── diff output ─────────────────────────────────────────\n\n";
    }

    for (const Diff& d : diffs) {
        switch (d.operation) {
            case DELETE:
                deletedChars += static_cast<int>(d.text.size());
                if (color) std::cout << RED;
                // Prefix each line so it's unambiguous in non-color output too
                {
                    std::istringstream ss(d.text);
                    std::string line;
                    bool first = true;
                    while (std::getline(ss, line)) {
                        if (!first) std::cout << "\n";
                        std::cout << "[-" << line << "-]";
                        first = false;
                    }
                    // Preserve trailing newline when original text ended with one
                    if (!d.text.empty() && d.text.back() == '\n') std::cout << "\n";
                }
                if (color) std::cout << RESET;
                break;

            case INSERT:
                insertedChars += static_cast<int>(d.text.size());
                if (color) std::cout << GREEN;
                {
                    std::istringstream ss(d.text);
                    std::string line;
                    bool first = true;
                    while (std::getline(ss, line)) {
                        if (!first) std::cout << "\n";
                        std::cout << "{+" << line << "+}";
                        first = false;
                    }
                    if (!d.text.empty() && d.text.back() == '\n') std::cout << "\n";
                }
                if (color) std::cout << RESET;
                break;

            case EQUAL:
                std::cout << d.text;
                break;
        }
    }

    std::cout << "\n";

    // --- summary ---
    if (color) {
        std::cout << BOLD
                  << "── summary ──────────────────────────────────────────────"
                  << RESET << "\n";
        std::cout << RED   << "  deleted : " << deletedChars  << " char(s)" << RESET << "\n";
        std::cout << GREEN << "  inserted: " << insertedChars << " char(s)" << RESET << "\n";
    } else {
        std::cout << "── summary ──────────────────────────────────────────────\n";
        std::cout << "  deleted : " << deletedChars  << " char(s)\n";
        std::cout << "  inserted: " << insertedChars << " char(s)\n";
    }

    if (deletedChars == 0 && insertedChars == 0) {
        std::cout << "  Files are identical.\n";
    }

    std::cout << "\n";
}

// ── entry point ───────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <file1> <file2>\n";
        return EXIT_FAILURE;
    }

    std::string text1, text2;
    if (!readFile(argv[1], text1)) return EXIT_FAILURE;
    if (!readFile(argv[2], text2)) return EXIT_FAILURE;

    diff_match_patch dmp;
    std::vector<Diff> diffs = dmp.diff_main(text1, text2);
    dmp.diff_cleanupSemantic(diffs);

    const bool color = supportsColor();

    if (color) {
        std::cout << BOLD << "File 1: " << RESET << argv[1] << "\n";
        std::cout << BOLD << "File 2: " << RESET << argv[2] << "\n";
    } else {
        std::cout << "File 1: " << argv[1] << "\n";
        std::cout << "File 2: " << argv[2] << "\n";
    }

    renderDiffs(diffs, color);

    return EXIT_SUCCESS;
}
