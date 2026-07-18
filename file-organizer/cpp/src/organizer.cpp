#include "organizer.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace organizer {
namespace {

// Extension -> category. Built once from the same table organize.py declares;
// a flat map keeps categorize() a single hash lookup rather than a scan.
const std::unordered_map<std::string, std::string>& extension_map() {
    static const std::unordered_map<std::string, std::string> map = [] {
        const std::vector<std::pair<const char*, std::vector<const char*>>> categories = {
            {"Images",    {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".svg", ".webp", ".tiff"}},
            {"Documents", {".pdf", ".doc", ".docx", ".txt", ".rtf", ".odt", ".md", ".xlsx", ".csv"}},
            {"Audio",     {".mp3", ".wav", ".flac", ".aac", ".ogg", ".m4a"}},
            {"Video",     {".mp4", ".mov", ".avi", ".mkv", ".webm", ".flv"}},
            {"Archives",  {".zip", ".tar", ".gz", ".rar", ".7z", ".bz2"}},
            {"Code",      {".py", ".js", ".html", ".css", ".java", ".cpp", ".c", ".json", ".sh"}},
        };
        std::unordered_map<std::string, std::string> m;
        for (const auto& [name, extensions] : categories)
            for (const char* ext : extensions) m.emplace(ext, name);
        return m;
    }();
    return map;
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// std::filesystem::path::string() is UTF-8 on POSIX but the active code page on
// Windows; u8string keeps log files identical across platforms.
std::string path_to_utf8(const fs::path& p) {
    const auto u8 = p.u8string();
    return std::string(u8.begin(), u8.end());
}

fs::path utf8_to_path(const std::string& s) {
    return fs::path(std::u8string(s.begin(), s.end()));
}

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (const char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// Read a JSON string literal starting at the opening quote. Returns the decoded
// value and advances `i` past the closing quote.
std::string read_json_string(const std::string& s, size_t& i) {
    if (i >= s.size() || s[i] != '"') throw std::runtime_error("expected '\"' in log file");
    ++i;
    std::string out;
    while (i < s.size() && s[i] != '"') {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            switch (s[i]) {
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                // Default covers an escaped quote and an escaped backslash.
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
        ++i;
    }
    if (i >= s.size()) throw std::runtime_error("unterminated string in log file");
    ++i;  // closing quote
    return out;
}

}  // namespace

const std::vector<std::string>& category_names() {
    static const std::vector<std::string> names = {
        "Images", "Documents", "Audio", "Video", "Archives", "Code",
    };
    return names;
}

bool is_category_folder(const std::string& name) {
    if (name == kOtherFolder) return true;
    const auto& names = category_names();
    return std::find(names.begin(), names.end(), name) != names.end();
}

std::string categorize(const fs::path& file) {
    const auto ext = to_lower(path_to_utf8(file.extension()));
    const auto& map = extension_map();
    const auto it = map.find(ext);
    return it != map.end() ? it->second : kOtherFolder;
}

fs::path resolve_collision(const fs::path& destination) {
    if (!fs::exists(destination)) return destination;

    const auto stem = path_to_utf8(destination.stem());
    const auto suffix = path_to_utf8(destination.extension());
    const auto parent = destination.parent_path();

    for (int counter = 1;; ++counter) {
        auto candidate = parent / utf8_to_path(stem + "_" + std::to_string(counter) + suffix);
        if (!fs::exists(candidate)) return candidate;
    }
}

std::vector<Move> plan_moves(const fs::path& folder) {
    if (!fs::is_directory(folder))
        throw std::runtime_error("'" + path_to_utf8(folder) + "' is not a valid directory.");

    // Collect first and sort by name. directory_iterator order is unspecified,
    // and a tool that reports a different order every run is hard to trust.
    std::vector<fs::path> entries;
    for (const auto& entry : fs::directory_iterator(folder)) {
        if (entry.is_directory()) continue;                      // includes our own output folders
        const auto name = path_to_utf8(entry.path().filename());
        if (!name.empty() && name.front() == '.') continue;       // hidden files
        entries.push_back(entry.path());
    }
    std::sort(entries.begin(), entries.end(), [](const fs::path& a, const fs::path& b) {
        return path_to_utf8(a.filename()) < path_to_utf8(b.filename());
    });

    // Track names claimed by this plan as well as those already on disk, so two
    // files landing in the same category can't be planned onto one destination.
    std::unordered_set<std::string> claimed;
    std::vector<Move> moves;

    for (const auto& source : entries) {
        const auto destination_folder = folder / categorize(source);
        auto destination = resolve_collision(destination_folder / source.filename());

        while (claimed.count(path_to_utf8(destination))) {
            // resolve_collision only checks the filesystem; step past names this
            // same plan has already taken.
            const auto stem = path_to_utf8(destination.stem());
            const auto suffix = path_to_utf8(destination.extension());
            int counter = 1;
            fs::path candidate;
            do {
                candidate = destination_folder /
                            utf8_to_path(path_to_utf8(source.stem()) + "_" +
                                         std::to_string(counter) + suffix);
                ++counter;
            } while (fs::exists(candidate) || claimed.count(path_to_utf8(candidate)));
            destination = candidate;
        }

        claimed.insert(path_to_utf8(destination));
        moves.push_back({source, destination});
    }

    return moves;
}

void apply_move(const Move& move) {
    const auto parent = move.destination.parent_path();
    if (!parent.empty()) fs::create_directories(parent);

    // The fast path. rename() also fails across volumes, so a failure here is
    // not necessarily fatal — fall through and try copy+delete like shutil.move.
    std::error_code ec;
    fs::rename(move.source, move.destination, ec);
    if (!ec) return;

    std::error_code copy_ec;
    fs::copy_file(move.source, move.destination, fs::copy_options::overwrite_existing, copy_ec);
    if (copy_ec) {
        // Common on Windows when another process holds the file open. Report
        // which file and why rather than letting the exception escape untyped.
        throw std::runtime_error("could not move '" + path_to_utf8(move.source) + "' to '" +
                                 path_to_utf8(move.destination) + "': " + copy_ec.message());
    }

    std::error_code remove_ec;
    fs::remove(move.source, remove_ec);
    if (remove_ec) {
        // The copy landed but the original won't go. Roll the copy back so the
        // file exists exactly once rather than silently in both places.
        fs::remove(move.destination, ec);
        throw std::runtime_error("copied '" + path_to_utf8(move.source) +
                                 "' but could not remove the original: " + remove_ec.message());
    }
}

std::vector<Move> apply_moves(const std::vector<Move>& moves) {
    std::vector<Move> performed;
    performed.reserve(moves.size());

    for (const auto& move : moves) {
        apply_move(move);
        performed.push_back(move);
    }

    return performed;
}

std::vector<Move> undo_moves(const std::vector<Move>& moves) {
    std::vector<Move> reversed;

    for (const auto& move : moves) {
        // The file may have been moved or deleted since the log was written;
        // skip it rather than aborting the rest of the undo.
        if (!fs::exists(move.destination)) continue;

        const Move back{move.destination, resolve_collision(move.source)};
        apply_move(back);
        reversed.push_back(back);
    }

    return reversed;
}

std::string to_json(const std::vector<Move>& moves) {
    std::ostringstream out;
    out << "[\n";
    for (size_t i = 0; i < moves.size(); ++i) {
        out << "  {\n"
            << "    \"from\": \"" << json_escape(path_to_utf8(moves[i].source)) << "\",\n"
            << "    \"to\": \"" << json_escape(path_to_utf8(moves[i].destination)) << "\"\n"
            << "  }" << (i + 1 < moves.size() ? "," : "") << "\n";
    }
    out << "]";
    return out.str();
}

std::vector<Move> from_json(const std::string& json) {
    std::vector<Move> moves;
    size_t i = 0;

    // The log is a fixed shape we control, so scan for the two known keys
    // rather than pulling in a general-purpose JSON dependency.
    while (i < json.size()) {
        const auto from_key = json.find("\"from\"", i);
        if (from_key == std::string::npos) break;

        auto p = json.find(':', from_key);
        if (p == std::string::npos) throw std::runtime_error("malformed log: expected ':'");
        ++p;
        while (p < json.size() && std::isspace(static_cast<unsigned char>(json[p]))) ++p;
        const auto from = read_json_string(json, p);

        const auto to_key = json.find("\"to\"", p);
        if (to_key == std::string::npos) throw std::runtime_error("malformed log: missing \"to\"");
        p = json.find(':', to_key);
        if (p == std::string::npos) throw std::runtime_error("malformed log: expected ':'");
        ++p;
        while (p < json.size() && std::isspace(static_cast<unsigned char>(json[p]))) ++p;
        const auto to = read_json_string(json, p);

        moves.push_back({utf8_to_path(from), utf8_to_path(to)});
        i = p;
    }

    return moves;
}

}  // namespace organizer
