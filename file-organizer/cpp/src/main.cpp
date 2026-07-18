// main.cpp — CLI front end for the file organizer.
//
//   organize <folder>                 sort a folder into category subfolders
//   organize <folder> --dry-run       preview without touching anything
//   organize <folder> --undo LOG      reverse a previous run

#include "organizer.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string utf8(const fs::path& p) {
    const auto s = p.u8string();
    return std::string(s.begin(), s.end());
}

std::string timestamp_now() {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return out.str();
}

int run_organize(const fs::path& folder, bool dry_run) {
    const auto resolved = fs::absolute(folder).lexically_normal();

    std::vector<organizer::Move> moves;
    try {
        moves = organizer::plan_moves(resolved);
    } catch (const std::exception& ex) {
        std::cout << "Error: " << ex.what() << "\n";
        return 1;
    }

    if (moves.empty()) {
        std::cout << "No files to organize.\n";
        return 0;
    }

    std::cout << "\n" << (dry_run ? "DRY RUN - no changes made" : "Organizing") << ": "
              << utf8(resolved) << "\n"
              << std::string(60, '-') << "\n";

    std::map<std::string, int> summary;
    for (const auto& move : moves) {
        const auto category = utf8(move.destination.parent_path().filename());
        ++summary[category];
        std::cout << "  " << std::left << std::setw(40) << utf8(move.source.filename())
                  << " -> " << category << "/\n";
    }

    if (!dry_run) organizer::apply_moves(moves);

    std::cout << std::string(60, '-') << "\n" << "Summary:\n";
    for (const auto& [category, count] : summary)  // std::map iterates sorted
        std::cout << "  " << std::left << std::setw(12) << category << " " << count << " file(s)\n";
    std::cout << "  " << std::left << std::setw(12) << "Total" << " " << moves.size() << " file(s)\n";

    if (!dry_run) {
        const auto log_name = "organize_log_" + timestamp_now() + ".json";
        std::ofstream log(resolved / log_name, std::ios::binary);
        log << organizer::to_json(moves);
        std::cout << "\nLog saved to: " << log_name << "\n"
                  << "To undo: organize " << utf8(resolved) << " --undo " << log_name << "\n";
    }

    return 0;
}

int run_undo(const fs::path& folder, const std::string& log_file) {
    const auto resolved = fs::absolute(folder).lexically_normal();
    const auto log_path = resolved / log_file;

    if (!fs::exists(log_path)) {
        std::cout << "Error: log file '" << utf8(log_path) << "' not found.\n";
        return 1;
    }

    std::ifstream in(log_path, std::ios::binary);
    const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    std::vector<organizer::Move> moves;
    try {
        moves = organizer::from_json(json);
    } catch (const std::exception& ex) {
        std::cout << "Error: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "Undoing " << moves.size() << " move(s)...\n" << std::string(60, '-') << "\n";

    for (const auto& move : organizer::undo_moves(moves))
        std::cout << "  " << utf8(move.source.filename()) << " -> back to original location\n";

    std::cout << std::string(60, '-') << "\nUndo complete.\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);

    std::string folder;
    std::string undo_log;
    bool dry_run = false;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--dry-run") {
            dry_run = true;
        } else if (args[i] == "--undo" && i + 1 < args.size()) {
            undo_log = args[++i];
        } else if (args[i] == "-h" || args[i] == "--help") {
            std::cout << "usage: organize <folder> [--dry-run] [--undo LOG_FILE]\n";
            return 0;
        } else if (!args[i].empty() && args[i].front() == '-') {
            std::cerr << "Error: unknown option '" << args[i] << "'.\n";
            return 2;
        } else if (folder.empty()) {
            folder = args[i];
        }
    }

    if (folder.empty()) {
        std::cerr << "usage: organize <folder> [--dry-run] [--undo LOG_FILE]\n";
        return 2;
    }

    try {
        return undo_log.empty() ? run_organize(folder, dry_run) : run_undo(folder, undo_log);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
