#pragma once

/**
 * @file        path.hpp
 * @brief       Path queries and directory creation - POSIX, no exceptions
 * @description The fourth piece of ase-fileio, after text_reader.hpp, text_writer.hpp and
 *              directory.hpp. Answers the questions a caller asks BEFORE it reads, writes
 *              or lists: does this exist, is it a directory, what contains it, and make
 *              the containers if they are missing.
 *
 *              WHY THESE FOUR AND NOT NINE. A second implementation of the same domain
 *              exists in the tree - `ase::utils::fs` - and the stakeholder decided that
 *              ase::fileio is the destination and that one is not, because it is a wrapper
 *              around the very facility the rules point away from: a wrapper hides a
 *              coupling instead of dissolving it. What the tree actually CALLS was measured
 *              before this file was written (by the session whose 27 files migrate onto it):
 *
 *                exists              15 calls in 7 files   → path_exists
 *                create_directories   7 calls in 5 files   → create_directories
 *                parent_of            2 calls in 1 file    → parent_of
 *                is_directory         1 call               → is_directory
 *                list_directory       1 call               → list_dir (directory.hpp)
 *
 *              FOUR FUNCTIONS THERE HAVE NO CALLER AT ALL - filename_of, is_regular_file,
 *              relative_to, remove. They are deliberately absent here. Porting them would
 *              be work with no consumer, and every one of them would then need maintaining
 *              against a caller that never arrives.
 *
 *              THE TRAP THIS FILE EXISTS TO CLOSE, and it is silent: the older module's
 *              `exists` is true for ANY entry, while `file_exists` in text_reader.hpp is
 *              true only for a REGULAR FILE - a directory gives false. BOTH RETURN bool,
 *              so no compiler will ever tell a caller it picked the wrong one. That is why
 *              they carry different names and both headers say which question they answer.
 *              A caller migrating off a filesystem-style `exists` wants `path_exists`
 *              here; a caller guarding a read wants `file_exists` there.
 *
 * @module      ase-fileio
 * @layer       0 (Foundation)
 * @category    structure/filesystem/io
 * @created     2026-08-22
 * @modified    2026-08-22
 * @version     1.0.0
 */

#include <sys/stat.h>

#include <cerrno>
#include <string>

namespace ase::fileio {

/// Permission bits for a directory this module creates: owner full, group and world may
/// enter and list. The process umask still applies on top.
constexpr unsigned int DIR_CREATE_MODE = 0755u;

/**
 * True if ANY entry exists at path - file, directory, symlink target, socket. Symlinks
 * are followed, so a dangling link reports false.
 *
 * This is the WIDER of the two existence questions. The narrower one, "is there a regular
 * file here", is file_exists in text_reader.hpp. See the trap note in the header block.
 */
inline bool path_exists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

/**
 * True if path names a directory. Symlinks are followed, so a link to a directory reports
 * true. A missing path reports false rather than an error - every measured caller uses
 * this as a guard, and none of them distinguishes "not a directory" from "not there".
 */
inline bool is_directory(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

/**
 * The containing directory of path, as a string. Purely lexical - the path need not
 * exist, and nothing is resolved or normalised.
 *
 *     "a/b/c"  → "a/b"        "/foo"  → "/"        "foo"  → ""
 *     "a/b/"   → "a/b"        "/"     → "/"        ""     → ""
 *
 * THE TRAILING-SLASH LINE IS THE ONE THAT MATTERS, and it is not an oversight: "a/b/" has
 * an empty final component, so its parent is "a/b", not "a". That is what the facility
 * this replaces does, and both callers in log_files.cpp feed the result
 * straight into create_directories - a silent shift by one level would create the wrong
 * directory rather than fail visibly.
 *
 * A degenerate run of separators is NOT collapsed: "a/b//" yields "a/b/". No caller in the
 * tree produces one, and inventing a normalisation here would be a second truth about what
 * a path means.
 */
inline std::string parent_of(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) return {};  // no separator at all: no parent
    if (slash == 0u) return "/";                // "/foo" and "/" both live at the root
    return path.substr(0, slash);
}

/**
 * Join two path segments with exactly one separator.
 *
 *     path_join("/a", "b")   → "/a/b"        path_join("", "b")    → "b"
 *     path_join("/a/", "b")  → "/a/b"        path_join("/a", "")   → "/a"
 *
 * DIE SEMANTIK IST NICHT ERFUNDEN, sondern aus der einzigen Fassung uebernommen, die der Baum
 * am 2026-08-22 hatte: `path_join` in `clients/ase-client-viewer/src/fs_utils.cpp`. Sie liegt dort
 * client-lokal in `ase::viewer::fs` und hat sechs Aufrufstellen; ein zweiter Client kann sie
 * nicht benutzen, ohne eine Kante zwischen zwei Clients aufzumachen. **Deshalb steht sie hier
 * und nicht noch einmal dort** — der Viewer kann spaeter darauf wandern, das ist nicht Teil
 * dieses Zuges.
 *
 * EIN UNTERSCHIED ZUR PFADBIBLIOTHEK, ausdruecklich: deren Verkettungsoperator VERWIRFT den
 * linken Teil, wenn der rechte absolut ist. Diese Fassung tut das nicht — `path_join("/a",
 * "/b")` ergibt "/a//b". Der doppelte Trenner faellt in `absolute_normalized` weg; wer join
 * ohne Normalisierung benutzt, bekommt ihn zu sehen. Das ist die Fassung, die der Baum heute
 * faehrt, und sie zu aendern waere eine stille Verhaltensaenderung an sechs fremden Stellen.
 */
inline std::string path_join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (a.back() == '/') return a + b;
    return a + "/" + b;
}

/**
 * Make a path absolute against `base` and collapse "." and ".." lexically.
 *
 *     absolute_normalized("docs/../src", "/home/x")  → "/home/x/src"
 *     absolute_normalized("/a/b/../c",   egal)       → "/a/c"
 *     absolute_normalized("a//b/./c",    "/r")       → "/r/a/b/c"
 *
 * WARUM `base` EIN PARAMETER IST UND NICHT DAS ARBEITSVERZEICHNIS: die Pfadbibliothek nimmt
 * dort still den Prozesszustand. Ein Ergebnis, das vom Arbeitsverzeichnis abhaengt, ist weder
 * pruefbar noch vorhersagbar — und ein `cd` von frueher wirkt in einer langlebigen Sitzung
 * nach. Der Rufer holt es sich mit `fileio::current_dir` (directory.hpp) und uebergibt es;
 * damit bleibt diese Datei frei von Prozesszustand UND frei von einer Abhaengigkeit auf
 * directory.hpp.
 *
 * REIN LEXIKALISCH: kein Symlink wird aufgeloest, kein Zugriff auf das Dateisystem. Ein ".."
 * hinter einem Symlink fuehrt deshalb woandershin als bei einer aufloesenden Fassung. Der Baum
 * hatte am 2026-08-22 genau ZWEI Stellen dieser Art (beide in `folder_picker.cpp`), beide auf
 * Verzeichnissen, die der Benutzer gerade angeklickt hat — dort gibt es nichts aufzuloesen.
 *
 * Ein ".." ueber die Wurzel hinaus bleibt an der Wurzel: "/.." ergibt "/".
 */
inline std::string absolute_normalized(const std::string& path, const std::string& base) {
    const std::string full = (!path.empty() && path[0] == '/') ? path : path_join(base, path);

    // Segmentweise auflegen: leere Stuecke und "." fallen weg, ".." nimmt das letzte zurueck.
    std::string out;
    out.reserve(full.size() + 1u);
    size_t start = 0u;
    while (start <= full.size()) {
        const size_t sep = full.find('/', start);
        const size_t end = (sep == std::string::npos) ? full.size() : sep;
        const size_t len = end - start;

        if (len == 0u || (len == 1u && full[start] == '.')) {
            // leeres Segment oder "." - beides traegt nichts bei
        } else if (len == 2u && full[start] == '.' && full[start + 1u] == '.') {
            const size_t cut = out.find_last_of('/');
            if (cut != std::string::npos) out.erase(cut);  // an der Wurzel bleibt out leer
        } else {
            out += '/';
            out.append(full, start, len);
        }

        if (sep == std::string::npos) break;
        start = sep + 1u;
    }

    return out.empty() ? std::string("/") : out;
}

/**
 * Create path and every missing directory above it. Returns true when path is a directory
 * afterwards.
 *
 * ALREADY-EXISTS IS SUCCESS, and that is not laxity - it is what the callers depend on.
 * The facility this replaces reports through an error code that stays clear when the
 * directory is already there, so every measured call site treats "it exists" and "I made
 * it" as the same outcome. Two of them discard the result entirely - both spell it
 * `(void)utils::fs::create_directories(...)` in log_files.cpp; returning false for an
 * existing directory would break them silently.
 *
 * THE FINAL CHECK IS NOT REDUNDANT. mkdir reports EEXIST for a REGULAR FILE at that path
 * just as it does for a directory, so a loop that only skips EEXIST would report success
 * for a path that can never hold anything. The closing is_directory is what makes the
 * return value mean what it says.
 */
inline bool create_directories(const std::string& path) {
    if (path.empty()) return false;

    // Walk the prefixes left to right, creating each one that is missing. The bound is
    // path.size() inclusive so the last component - which has no separator after it - gets
    // its own turn.
    for (size_t i = 1u; i <= path.size(); ++i) {
        if (i < path.size() && path[i] != '/') continue;

        const std::string prefix = path.substr(0, i);
        if (prefix == "/") continue;  // the root is never created

        if (::mkdir(prefix.c_str(), static_cast<mode_t>(DIR_CREATE_MODE)) == 0) continue;
        if (errno == EEXIST) continue;  // someone was faster, or it was always there
        return false;
    }

    return is_directory(path);
}

}  // namespace ase::fileio
