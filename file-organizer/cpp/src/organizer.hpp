// organizer.hpp — file categorization and collision-safe move planning.
//
// The rules here mirror organize.py exactly: the same extension map, the same
// "Other" fallback, and the same stem_N.ext collision scheme. Planning is kept
// separate from execution so a dry run and a real run share one code path and
// can't drift apart.

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace organizer {

/// One planned relocation. Produced by plan_moves, consumed by apply_moves.
struct Move {
    std::filesystem::path source;
    std::filesystem::path destination;
};

/// Folder that receives anything with an unrecognized extension.
inline constexpr const char* kOtherFolder = "Other";

/// The category folder names, in the order organize.py declares them.
const std::vector<std::string>& category_names();

/// Every folder this tool creates, including "Other" — used to skip its own output.
bool is_category_folder(const std::string& name);

/// Category folder for a file, chosen by lowercased extension.
std::string categorize(const std::filesystem::path& file);

/// If `destination` is taken, return destination with _1, _2, ... appended to
/// the stem until it is free. Never returns a path that already exists.
std::filesystem::path resolve_collision(const std::filesystem::path& destination);

/// Decide where every eligible file in `folder` should go. Directories and
/// dotfiles are skipped, as is anything already inside a category folder.
/// Entries are sorted by filename so a run is reproducible.
std::vector<Move> plan_moves(const std::filesystem::path& folder);

/// Execute one relocation, creating the destination folder if needed.
void apply_move(const Move& move);

/// Execute a plan, creating category folders as needed. Returns moves performed.
std::vector<Move> apply_moves(const std::vector<Move>& moves);

/// Reverse a plan, putting every file back where it came from. Files that are
/// no longer at their destination are skipped rather than failing the undo.
/// Returns the moves actually reversed.
std::vector<Move> undo_moves(const std::vector<Move>& moves);

/// Serialize a plan to the same JSON shape organize.py writes, so a log from
/// either implementation can be undone by either.
std::string to_json(const std::vector<Move>& moves);

/// Parse a log written by to_json (or by organize.py).
std::vector<Move> from_json(const std::string& json);

}  // namespace organizer
