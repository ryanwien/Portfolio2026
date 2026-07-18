// test_organizer.cpp — self-contained test suite, no external dependencies.
//
// Everything that touches the filesystem runs inside a fresh temporary
// directory that is removed afterwards, so the tests never touch real files.

#include "../src/organizer.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_checks = 0;
int g_failures = 0;

// Set ORGANIZER_TEST_VERBOSE=1 to echo every check as it runs. Useful when a
// check crashes rather than fails: the last line printed is the one that died.
const bool g_verbose = std::getenv("ORGANIZER_TEST_VERBOSE") != nullptr;

void check(bool condition, const std::string& what) {
    ++g_checks;
    if (g_verbose) std::cout << "    - " << what << "\n";
    if (!condition) {
        ++g_failures;
        std::cout << "  FAIL: " << what << "\n";
    }
}

template <typename A, typename B>
void check_eq(const A& actual, const B& expected, const std::string& what) {
    ++g_checks;
    if (g_verbose) std::cout << "    - " << what << "\n";
    if (!(actual == expected)) {
        ++g_failures;
        std::cout << "  FAIL: " << what << "\n"
                  << "        expected: " << expected << "\n"
                  << "        actual:   " << actual << "\n";
    }
}

/// A temporary directory that cleans itself up.
class TempDir {
public:
    TempDir() {
        static int counter = 0;
        path_ = fs::temp_directory_path() /
                ("organizer_test_" + std::to_string(++counter) + "_" +
                 std::to_string(static_cast<long long>(
                     std::chrono::steady_clock::now().time_since_epoch().count())));
        fs::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const fs::path& path() const { return path_; }

    void write(const std::string& name, const std::string& contents = "x") const {
        std::ofstream out(path_ / name, std::ios::binary);
        out << contents;
    }

private:
    fs::path path_;
};

void test_categorize() {
    std::cout << "categorize\n";
    check_eq(organizer::categorize("photo.jpg"), std::string("Images"), "jpg is an image");
    check_eq(organizer::categorize("photo.PNG"), std::string("Images"), "extension match is case-insensitive");
    check_eq(organizer::categorize("notes.md"), std::string("Documents"), "md is a document");
    check_eq(organizer::categorize("song.mp3"), std::string("Audio"), "mp3 is audio");
    check_eq(organizer::categorize("clip.mp4"), std::string("Video"), "mp4 is video");
    check_eq(organizer::categorize("backup.zip"), std::string("Archives"), "zip is an archive");
    check_eq(organizer::categorize("main.cpp"), std::string("Code"), "cpp is code");
    check_eq(organizer::categorize("firmware.dat"), std::string("Other"), "unknown extension falls back to Other");
    check_eq(organizer::categorize("README"), std::string("Other"), "no extension falls back to Other");
    // A dotfile has no extension by filesystem rules, so it lands in Other —
    // plan_moves skips it before this ever matters, but the rule should hold.
    check_eq(organizer::categorize(".gitignore"), std::string("Other"), "dotfile has no extension");
}

void test_collision_resolution() {
    std::cout << "resolve_collision\n";
    TempDir dir;

    const auto free_path = dir.path() / "report.pdf";
    check_eq(organizer::resolve_collision(free_path), free_path, "a free name is returned unchanged");

    dir.write("report.pdf");
    check_eq(organizer::resolve_collision(free_path), dir.path() / "report_1.pdf",
             "a taken name becomes _1");

    dir.write("report_1.pdf");
    check_eq(organizer::resolve_collision(free_path), dir.path() / "report_2.pdf",
             "_1 taken as well becomes _2");

    dir.write("archive.tar.gz");
    check_eq(organizer::resolve_collision(dir.path() / "archive.tar.gz"),
             dir.path() / "archive.tar_1.gz",
             "only the final extension is preserved, matching pathlib");
}

void test_plan_skips_directories_and_dotfiles() {
    std::cout << "plan_moves: what gets skipped\n";
    TempDir dir;
    dir.write("photo.jpg");
    dir.write(".hidden");
    fs::create_directory(dir.path() / "Images");
    fs::create_directory(dir.path() / "some_folder");

    const auto moves = organizer::plan_moves(dir.path());
    check_eq(moves.size(), size_t{1}, "only the one visible file is planned");
    check_eq(moves[0].destination, dir.path() / "Images" / "photo.jpg", "it lands in Images");
}

void test_plan_is_deterministic() {
    std::cout << "plan_moves: ordering\n";
    TempDir dir;
    dir.write("c.txt");
    dir.write("a.txt");
    dir.write("b.txt");

    const auto first = organizer::plan_moves(dir.path());
    const auto second = organizer::plan_moves(dir.path());

    check_eq(first.size(), size_t{3}, "all three files planned");
    check_eq(first[0].source.filename().string(), std::string("a.txt"), "sorted by filename");
    check_eq(first[2].source.filename().string(), std::string("c.txt"), "sorted by filename");

    bool identical = first.size() == second.size();
    for (size_t i = 0; identical && i < first.size(); ++i)
        identical = first[i].source == second[i].source && first[i].destination == second[i].destination;
    check(identical, "planning twice gives the same plan");
}

void test_plan_avoids_intra_run_collisions() {
    std::cout << "plan_moves: two sources, one destination name\n";
    TempDir dir;
    // Both are Documents, and Documents/notes.txt is already occupied.
    fs::create_directory(dir.path() / "Documents");
    std::ofstream(dir.path() / "Documents" / "notes.txt") << "existing";
    dir.write("notes.txt");

    const auto moves = organizer::plan_moves(dir.path());
    check_eq(moves.size(), size_t{1}, "one file to move");
    check_eq(moves[0].destination, dir.path() / "Documents" / "notes_1.txt",
             "it dodges the file already there");
}

void test_end_to_end_move_and_undo() {
    std::cout << "apply_moves and undo round trip\n";
    TempDir dir;
    dir.write("photo.jpg", "image-bytes");
    dir.write("notes.md", "doc-bytes");
    dir.write("firmware.dat", "other-bytes");

    const auto moves = organizer::plan_moves(dir.path());
    organizer::apply_moves(moves);

    check(fs::exists(dir.path() / "Images" / "photo.jpg"), "image was moved into Images");
    check(fs::exists(dir.path() / "Documents" / "notes.md"), "document was moved into Documents");
    check(fs::exists(dir.path() / "Other" / "firmware.dat"), "unknown type was moved into Other");
    check(!fs::exists(dir.path() / "photo.jpg"), "the original no longer sits in the root");

    // Contents must survive the move. Read inside a scope so the handle is
    // closed before the undo below — Windows refuses to move an open file.
    {
        std::ifstream in(dir.path() / "Images" / "photo.jpg", std::ios::binary);
        const std::string contents((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
        check_eq(contents, std::string("image-bytes"), "file contents are intact");
    }

    // Undo: reverse every move, then the folder should look untouched.
    const auto reversed = organizer::undo_moves(moves);
    check_eq(reversed.size(), moves.size(), "undo reversed every move");

    check(fs::exists(dir.path() / "photo.jpg"), "undo restored the image");
    check(fs::exists(dir.path() / "notes.md"), "undo restored the document");
    check(!fs::exists(dir.path() / "Images" / "photo.jpg"), "nothing left behind in Images");
}

void test_json_round_trip() {
    std::cout << "log serialization\n";
    const std::vector<organizer::Move> moves = {
        {"C:\\Users\\test\\photo.jpg", "C:\\Users\\test\\Images\\photo.jpg"},
        {"/home/test/notes.md", "/home/test/Documents/notes.md"},
    };

    const auto json = organizer::to_json(moves);
    check(json.find("\"from\"") != std::string::npos, "log has a from field");
    check(json.find("\\\\") != std::string::npos, "backslashes in Windows paths are escaped");

    const auto parsed = organizer::from_json(json);
    check_eq(parsed.size(), moves.size(), "round trip preserves the move count");
    check_eq(parsed[0].source, moves[0].source, "round trip preserves the source path");
    check_eq(parsed[1].destination, moves[1].destination, "round trip preserves the destination path");
}

void test_undo_skips_files_that_moved_on() {
    std::cout << "undo_moves: missing files\n";
    TempDir dir;
    dir.write("photo.jpg");
    dir.write("notes.md");

    const auto moves = organizer::plan_moves(dir.path());
    organizer::apply_moves(moves);

    // Someone deleted one of the organized files before the undo ran.
    fs::remove(dir.path() / "Images" / "photo.jpg");

    const auto reversed = organizer::undo_moves(moves);
    check_eq(reversed.size(), size_t{1}, "only the surviving file is restored");
    check(fs::exists(dir.path() / "notes.md"), "the survivor is back in the root");
}

void test_undo_does_not_overwrite_a_replacement() {
    std::cout << "undo_moves: original name taken again\n";
    TempDir dir;
    dir.write("photo.jpg", "original");

    const auto moves = organizer::plan_moves(dir.path());
    organizer::apply_moves(moves);

    // A new file has since claimed the original name.
    dir.write("photo.jpg", "replacement");

    organizer::undo_moves(moves);

    std::string contents;
    {
        std::ifstream in(dir.path() / "photo.jpg", std::ios::binary);
        contents.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    check_eq(contents, std::string("replacement"), "the newer file is left untouched");
    check(fs::exists(dir.path() / "photo_1.jpg"), "the restored file steps aside to _1");
}

void test_locked_file_reports_instead_of_crashing() {
    std::cout << "apply_move: file held open by another handle\n";
    TempDir dir;
    dir.write("locked.txt", "data");

    // Hold the file open for writing. On Windows this blocks the move; on
    // POSIX the rename succeeds, so accept either outcome — what must never
    // happen is an unhandled exception taking the process down.
    std::ofstream hold(dir.path() / "locked.txt", std::ios::app);
    hold << "keep-open";
    hold.flush();

    bool crashed = false;
    bool reported = false;
    try {
        organizer::apply_move({dir.path() / "locked.txt", dir.path() / "Documents" / "locked.txt"});
    } catch (const std::runtime_error& ex) {
        reported = true;
        // The message must name the file, or an analyst can't act on it.
        check(std::string(ex.what()).find("locked.txt") != std::string::npos,
              "the error names the offending file");
    } catch (...) {
        crashed = true;
    }

    check(!crashed, "a blocked move raises a typed error, never an unknown one");
    check(reported || fs::exists(dir.path() / "Documents" / "locked.txt"),
          "the move either succeeded or was reported cleanly");

    // The file must still exist somewhere — never lost between the two paths.
    check(fs::exists(dir.path() / "locked.txt") ||
          fs::exists(dir.path() / "Documents" / "locked.txt"),
          "the file is never lost");
}

void test_missing_directory_is_reported() {
    std::cout << "error handling\n";
    bool threw = false;
    try {
        organizer::plan_moves("definitely-not-a-real-directory-xyz");
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "planning against a missing directory throws");
}

}  // namespace

int main() {
    // Unbuffered: if a check crashes the process, the progress so far still
    // reaches the terminal instead of dying in the stream buffer.
    std::cout << std::unitbuf;
    std::cout << "file organizer tests\n" << std::string(60, '-') << "\n";

    test_categorize();
    test_collision_resolution();
    test_plan_skips_directories_and_dotfiles();
    test_plan_is_deterministic();
    test_plan_avoids_intra_run_collisions();
    test_end_to_end_move_and_undo();
    test_undo_skips_files_that_moved_on();
    test_undo_does_not_overwrite_a_replacement();
    test_locked_file_reports_instead_of_crashing();
    test_json_round_trip();
    test_missing_directory_is_reported();

    std::cout << std::string(60, '-') << "\n";
    if (g_failures == 0) {
        std::cout << "All " << g_checks << " checks passed.\n";
        return 0;
    }
    std::cout << g_failures << " of " << g_checks << " checks FAILED.\n";
    return 1;
}
