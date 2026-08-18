"""
organize.py — Command-line file organizer

Sorts the files in a directory into subfolders by type (images, documents,
audio, video, archives, code, etc.) based on their file extension.

Features:
    - Dry-run mode to preview changes before doing anything
    - Configurable category rules (edit CATEGORIES below)
    - Safe handling of name collisions (never overwrites)
    - A summary report of what was moved
    - Undo support via a generated log file

Usage:
    python organize.py /path/to/folder              # organize a folder
    python organize.py /path/to/folder --dry-run    # preview only
    python organize.py /path/to/folder --undo log.json   # reverse a run

Built as a practical Python automation tool: argument parsing, file-system
operations, error handling, and a clean command-line interface.
"""

import argparse
import json
import shutil
import sys
from datetime import datetime
from pathlib import Path


# ---------------------------------------------------------------------------
# Category rules: which extensions go into which folder.
# Easy to extend — just add extensions or new categories.
# ---------------------------------------------------------------------------
CATEGORIES = {
    "Images":    {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".svg", ".webp", ".tiff"},
    "Documents": {".pdf", ".doc", ".docx", ".txt", ".rtf", ".odt", ".md", ".xlsx", ".csv"},
    "Audio":     {".mp3", ".wav", ".flac", ".aac", ".ogg", ".m4a"},
    "Video":     {".mp4", ".mov", ".avi", ".mkv", ".webm", ".flv"},
    "Archives":  {".zip", ".tar", ".gz", ".rar", ".7z", ".bz2"},
    "Code":      {".py", ".js", ".html", ".css", ".java", ".cpp", ".c", ".json", ".sh"},
}

# Anything whose extension isn't listed above goes here.
OTHER_FOLDER = "Other"


def categorize(file_path):
    """Return the category folder name for a given file.

    Looks up the file's extension (lowercased) in CATEGORIES and returns
    the matching category, or OTHER_FOLDER if no category matches.
    """
    extension = file_path.suffix.lower()
    for category, extensions in CATEGORIES.items():
        if extension in extensions:
            return category
    return OTHER_FOLDER


def resolve_collision(destination):
    """If a file with the same name already exists at the destination,
    append a number so nothing is ever overwritten.

    e.g. report.pdf -> report_1.pdf -> report_2.pdf
    """
    if not destination.exists():
        return destination

    stem = destination.stem
    suffix = destination.suffix
    parent = destination.parent
    counter = 1
    while True:
        candidate = parent / f"{stem}_{counter}{suffix}"
        if not candidate.exists():
            return candidate
        counter += 1


def organize(folder, dry_run=False):
    """Organize all files in `folder` into category subfolders.

    Returns a list of (source, destination) moves performed (or that
    would be performed in dry-run mode).
    """
    folder = Path(folder).expanduser().resolve()

    if not folder.is_dir():
        print(f"Error: '{folder}' is not a valid directory.")
        sys.exit(1)

    # Only process files directly in the folder, not files already inside
    # the category subfolders (avoids re-organizing our own output).
    category_names = set(CATEGORIES.keys()) | {OTHER_FOLDER}

    moves = []
    for item in folder.iterdir():
        # Skip directories (including our category folders) and hidden files.
        if item.is_dir() or item.name.startswith("."):
            continue

        category = categorize(item)
        dest_folder = folder / category
        destination = resolve_collision(dest_folder / item.name)
        moves.append((item, destination))

    if not moves:
        print("No files to organize.")
        return []

    # Report what we're about to do.
    print(f"\n{'DRY RUN — no changes made' if dry_run else 'Organizing'}: {folder}")
    print("-" * 60)

    summary = {}
    created_dirs = []
    for source, destination in moves:
        category = destination.parent.name
        summary[category] = summary.get(category, 0) + 1
        print(f"  {source.name:40s} -> {category}/")

        if not dry_run:
            # Remember only the folders this run brings into existence, so undo
            # can remove those and leave any the user already had.
            if not destination.parent.exists():
                created_dirs.append(str(destination.parent))
            destination.parent.mkdir(exist_ok=True)
            shutil.move(str(source), str(destination))

    # Print summary counts.
    print("-" * 60)
    print("Summary:")
    for category, count in sorted(summary.items()):
        print(f"  {category:12s} {count} file(s)")
    print(f"  {'Total':12s} {len(moves)} file(s)")

    # Save a log so the operation can be undone later.
    if not dry_run:
        log = {
            "moves": [{"from": str(src), "to": str(dst)} for src, dst in moves],
            "created_dirs": created_dirs,
        }
        log_path = folder / f"organize_log_{datetime.now():%Y%m%d_%H%M%S}.json"
        log_path.write_text(json.dumps(log, indent=2))
        print(f"\nLog saved to: {log_path.name}")
        print(f"To undo: python organize.py {folder} --undo {log_path.name}")

    return moves


def undo(folder, log_file):
    """Reverse a previous organize run using its log file."""
    folder = Path(folder).expanduser().resolve()
    log_path = folder / log_file

    if not log_path.exists():
        print(f"Error: log file '{log_path}' not found.")
        sys.exit(1)

    data = json.loads(log_path.read_text())
    # Logs written before undo learned to clean up are a bare list of moves.
    if isinstance(data, list):
        moves, created_dirs = data, []
    else:
        moves, created_dirs = data["moves"], data.get("created_dirs", [])

    print(f"Undoing {len(moves)} move(s)...")
    print("-" * 60)

    for entry in moves:
        source = Path(entry["to"])      # where the file is now
        destination = Path(entry["from"])  # where it came from
        if source.exists():
            destination = resolve_collision(destination)
            shutil.move(str(source), str(destination))
            print(f"  {source.name} -> back to original location")

    # Put the folder itself back too, not just the files in it. Only the
    # category folders this run created are considered, and only while empty,
    # so anything the user put there survives.
    removed = 0
    for dir_path in created_dirs:
        candidate = Path(dir_path)
        if candidate.is_dir() and not any(candidate.iterdir()):
            candidate.rmdir()
            removed += 1

    print("-" * 60)
    if removed:
        print(f"Undo complete. Removed {removed} empty folder(s) this run created.")
    else:
        print("Undo complete.")
    print(f"The log itself is left in place: {log_path.name}")


def main():
    parser = argparse.ArgumentParser(
        description="Organize files in a folder into subfolders by type."
    )
    parser.add_argument("folder", help="The folder to organize")
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Preview changes without moving any files",
    )
    parser.add_argument(
        "--undo",
        metavar="LOG_FILE",
        help="Reverse a previous run using its log file",
    )
    args = parser.parse_args()

    if args.undo:
        undo(args.folder, args.undo)
    else:
        organize(args.folder, dry_run=args.dry_run)


if __name__ == "__main__":
    main()
