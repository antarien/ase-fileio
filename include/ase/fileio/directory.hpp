#pragma once

/**
 * @file        directory.hpp
 * @brief       Directory listing and single-file removal - flat, POSIX, no exceptions
 * @description The directory side of ase-fileio, the third piece after text_reader.hpp
 *              (2026-04-13) and text_writer.hpp (2026-08-20). Same promises as both:
 *              errors are signalled by the return value, never by an exception, and the
 *              module exists so that its callers do not have to name the underlying API.
 *
 *              BUILT ON POSIX, AND THAT IS THE REASON IT IS WRITTEN TODAY RATHER THAN
 *              LATER. The two existing headers are built with the stream and path
 *              facilities of the standard library - with exactly what the forbidden-type
 *              rules point AT this module to avoid. A new capability resting on the same
 *              base would have to be written twice: once now, once when that is undone.
 *
 *              POSIX is established in the tree, measured before this file was written:
 *              foundation/ase-utils/include/ase/utils/clock.hpp calls ::time(nullptr),
 *              and ase-utils carries no finding for it - its only finding is the stream
 *              one, in a different function.
 *
 *              THE SIGNATURES COME FROM THE RULE, NOT FROM PREFERENCE. The rule
 *              STD_STRING_VIEW_FORBIDDEN says for Layer 0, verbatim: "Use `const char*`
 *              plus an explicit `uint32_t` length, which is what every existing L0 string
 *              helper does", and it names the standard string class as "WHAT IS NEVER THE
 *              ANSWER". The two existing headers take a view type and are reported for it;
 *              a new function in the same shape would have added the ninth finding to this
 *              module instead of none.
 *
 *              An output buffer rather than a returned container follows from the same
 *              sentence. A returned dynamic container would look green today only because
 *              no detector inspects a template argument - a blind spot, not a permission.
 *
 *              THE SHAPE IS THE CONSUMER'S, ASKED BEFORE IT WAS WRITTEN. The one caller
 *              that needs this is core/ase-log/src/log_files.cpp, prune_log_dir: it lists
 *              a log directory, drops entries older than a retention cutoff, and unlinks
 *              them. It asked for three fields and no fourth:
 *
 *                name            for the path and its own extension check
 *                modified_secs   the comparison against the cutoff
 *                is_regular      it tests this per entry; without it here, every entry
 *                                would need a second syscall at the call site
 *
 *              NOT here, each one refused with a reason: size (unused), recursion (the log
 *              directory has no subdirectories, and a retention sweep would not want to
 *              follow them), filtering (one line at the call site against a parameter every
 *              other caller would carry forever), sorting, and directory removal - the most
 *              dangerous function a file API can offer, and nobody asked for it.
 *
 *              int64_t Unix seconds rather than a clock type: a clock type forces every
 *              consumer into a time library to compare it, and POSIX stat() already
 *              delivers st_mtime in exactly this unit - no conversion anywhere in the
 *              chain. It is the same axis as ase::utils::wall_time_seconds().
 *
 * @module      ase-fileio
 * @layer       0 (Foundation)
 * @category    structure/filesystem/io
 * @created     2026-08-22
 * @modified    2026-08-22
 * @version     1.0.0
 */

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>

namespace ase::fileio {

/// Longest entry name this API reports. Linux NAME_MAX is 255; the extra byte is the
/// terminator, so no name the filesystem can produce is truncated.
constexpr uint32_t DIR_ENTRY_NAME_MAX = 256u;

/// Longest path this API accepts. Linux PATH_MAX is 4096; a caller passing more gets a
/// refusal rather than a silently shortened path pointing somewhere else.
///
/// IT STAYS HERE RATHER THAN IN path.hpp, and the attempt to move it is worth a line: this
/// is the buffer-facing half of the module, and path.hpp works entirely in strings and
/// needs no bound at all. Moving the constant would have bought one shared definition and
/// paid with a header dependency between two files that otherwise share nothing.
constexpr uint32_t DIR_PATH_MAX = 4096u;

/**
 * One directory entry: what it is called, when it was last written, and whether it is a
 * regular file. Nothing else - see the header note for what was refused and why.
 */
struct DirEntry {
    char    name[DIR_ENTRY_NAME_MAX] = {};     // NUL-terminated name, no leading path
    int64_t modified_secs            = 0;      // Unix seconds, straight from stat st_mtime
    bool    is_regular               = false;  // false for directories, links, sockets, ...
    bool    is_dir                   = false;  // true only for directories
    bool    is_symlink               = false;  // true when the ENTRY itself is a link
};

/*
 * WARUM DREI MARKEN UND NICHT EINE, gewachsen an je einem gemessenen Bedarf:
 *
 * `is_dir` ist NICHT `!is_regular`. Zwischen beiden liegen Symlinks, Sockets, FIFOs und
 * Geraetedateien — wer „alle Ordner" ueber `!is_regular` sucht, bekommt sie mit. Das Feld
 * kam am 2026-08-22 dazu, weil ein ZWEITER Verbraucher die komplementaere Frage stellt: der
 * Ordner-Waehler in clients/ase-client-explorer listet ausschliesslich Verzeichnisse
 * (folder_picker.cpp), waehrend der erste Verbraucher — die Logablage in
 * core/ase-log — ausschliesslich regulaere Dateien aufraeumt. **Die erste Fassung dieses
 * Typs war aus EINEM Verbraucher dimensioniert und trug deshalb nur seine Haelfte der
 * Frage.**
 *
 * `is_symlink` kam am selben Tag aus dem DRITTEN Verbraucher, und es ist die einzige Marke
 * hier, die etwas kostet — deshalb steht ihr Grund ausfuehrlich da:
 *
 *   Die beiden anderen Felder stammen aus stat(), und stat() FOLGT einem Symlink. Ein Link
 *   auf ein Verzeichnis meldet damit `is_dir == true`, und das ist fuer eine Abfrage („darf
 *   ich das anzeigen") richtig und fuer einen ABSTIEG falsch: eine rekursive Wanderung, die
 *   Links folgt, laeuft bei einem Zyklus (a/b zeigt auf a) unendlich. Die Pfadbibliothek der
 *   Standardbibliothek steigt aus genau diesem Grund per Vorgabe NICHT in Verzeichnislinks
 *   ab; wer sie hier ersetzt, muss dieselbe Grenze mitbringen, sonst tauscht er einen
 *   Verstoss gegen einen Haenger.
 *
 *   Die Antwort kommt aus lstat() und damit aus einem ZWEITEN Syscall je Eintrag. Das ist
 *   bewusst nicht ueber `d_type` aus readdir abgekuerzt: das Feld ist auf manchen
 *   Dateisystemen DT_UNKNOWN, und eine Marke, die je nach Dateisystem stimmt oder nicht,
 *   ist schlimmer als eine, die kostet — der Leser kann ihr nicht ansehen, welcher Fall
 *   gerade vorliegt.
 *
 *   Die anderen beiden Felder bleiben ausdruecklich auf stat(): ein Link auf eine
 *   regulaere Datei soll weiter `is_regular == true` melden, sonst aendert sich das
 *   Verhalten des Log-Verbrauchers still mit.
 */

namespace detail {

/**
 * Length of a NUL-terminated string, scanning at most `limit` bytes.
 *
 * Written out rather than taken from the C library because the rule
 * C_STRING_FUNCTIONS_FORBIDDEN forbids that family tree-wide, with no file filter. The
 * bound is not a consolation prize either: an unbounded scan over a name that the kernel
 * handed us is precisely the thing that should not be unbounded.
 *
 * @return the length, or `limit` when no terminator appears within the bound
 */
inline uint32_t bounded_length(const char* text, uint32_t limit) {
    uint32_t len = 0u;
    while (len < limit && text[len] != '\0') {
        ++len;
    }
    return len;
}

/**
 * Copy a bounded, possibly unterminated path into a NUL-terminated buffer.
 *
 * This exists because the interface takes (const char*, uint32_t) - the caller's path need
 * not be terminated, and every POSIX call needs it to be. Returns false when the path does
 * not fit, which is a refusal and not a shortening: a shortened path names a DIFFERENT
 * file, and silently operating on it is how a delete hits the wrong target.
 */
inline bool copy_path(const char* path, uint32_t path_len, char* out, uint32_t out_cap) {
    if (path == nullptr || out == nullptr || out_cap == 0u) return false;
    if (path_len >= out_cap) return false;
    for (uint32_t i = 0u; i < path_len; ++i) {
        out[i] = path[i];
    }
    out[path_len] = '\0';
    return true;
}

}  // namespace detail

/**
 * List a directory, flat, writing at most out_max entries into out.
 *
 * @param path      directory path, need not be NUL-terminated
 * @param path_len  length of path in bytes
 * @param out       caller-owned array receiving the entries
 * @param out_max   capacity of out, in entries
 * @return number of entries written
 *
 * "." and ".." are never reported. An unreadable or missing directory yields 0, the same as
 * an empty one - the single consumer treats both identically, and inventing a distinction
 * nobody uses would be a decision made on stock.
 *
 * An entry whose stat() fails is skipped rather than reported with a zero timestamp: zero
 * is 1970 and would look older than any retention cutoff, so a sweep would delete precisely
 * the files it could not inspect.
 *
 * When more entries exist than fit, the first out_max are written and the rest are not
 * reported. The return value equals out_max in that case, which is the caller's signal to
 * call again with more room if it cares.
 */
[[nodiscard]] inline uint32_t list_dir(const char* path, uint32_t path_len,
                                       DirEntry* out, uint32_t out_max) {
    if (out == nullptr || out_max == 0u) return 0u;

    char dir_path[DIR_PATH_MAX];
    if (!detail::copy_path(path, path_len, dir_path, DIR_PATH_MAX)) return 0u;

    DIR* dir = ::opendir(dir_path);
    if (dir == nullptr) return 0u;

    const uint32_t dir_len   = detail::bounded_length(dir_path, DIR_PATH_MAX);
    const bool     has_slash = dir_len > 0u && dir_path[dir_len - 1u] == '/';

    uint32_t written = 0u;
    for (struct dirent* ent = ::readdir(dir); ent != nullptr && written < out_max;
         ent = ::readdir(dir)) {
        const char* name = ent->d_name;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
            continue;  // "." and ".."
        }

        const uint32_t name_len = detail::bounded_length(name, DIR_ENTRY_NAME_MAX);
        if (name_len >= DIR_ENTRY_NAME_MAX) continue;  // no terminator within the bound

        // stat() needs the full path, not the bare entry name.
        const uint32_t needed = dir_len + (has_slash ? 0u : 1u) + name_len + 1u;
        if (needed > DIR_PATH_MAX) continue;

        char full[DIR_PATH_MAX];
        uint32_t pos = 0u;
        for (; pos < dir_len; ++pos) {
            full[pos] = dir_path[pos];
        }
        if (!has_slash) full[pos++] = '/';
        for (uint32_t i = 0u; i < name_len; ++i) {
            full[pos + i] = name[i];
        }
        full[pos + name_len] = '\0';

        struct stat st;
        if (::stat(full, &st) != 0) continue;  // unreadable: skip, never report as epoch 0

        // Second call on purpose: stat() above followed a link, lstat() does not, and only
        // the difference between the two answers the question "is the ENTRY a link". A
        // failure here is not a reason to drop the entry - it stays reported, with the
        // conservative answer "not a link", which is the one that keeps a walker walking.
        struct stat link_st;
        const bool  link_known = ::lstat(full, &link_st) == 0;

        DirEntry& slot = out[written];
        for (uint32_t i = 0u; i < name_len; ++i) {
            slot.name[i] = name[i];
        }
        slot.name[name_len] = '\0';
        slot.modified_secs  = static_cast<int64_t>(st.st_mtime);
        slot.is_symlink     = link_known && S_ISLNK(link_st.st_mode);
        slot.is_regular     = S_ISREG(st.st_mode);
        slot.is_dir         = S_ISDIR(st.st_mode);
        ++written;
    }

    ::closedir(dir);
    return written;
}

/**
 * Remove a single file.
 *
 * @param path      file path, need not be NUL-terminated
 * @param path_len  length of path in bytes
 * @return true when the file is gone afterwards - including when it was already gone
 *
 * IDEMPOTENT ON PURPOSE, and the consumer's code is the reason: prune_log_dir takes an
 * error code from its remove call and does not examine it. The sweep wants the file gone;
 * whether it was already gone is not a failure. Returning false there would force every
 * caller to separate "was not there" from "could not remove", and the one caller does not.
 *
 * Directories are never removed: ::unlink refuses them, and this function does not reach
 * for ::rmdir. Nobody asked for it, and it is the most dangerous thing a file API can do.
 */
[[nodiscard]] inline bool remove_file(const char* path, uint32_t path_len) {
    char file_path[DIR_PATH_MAX];
    if (!detail::copy_path(path, path_len, file_path, DIR_PATH_MAX)) return false;

    if (::unlink(file_path) == 0) return true;

    // Already gone is success. Anything else - permission, directory, busy - is not.
    struct stat st;
    return ::stat(file_path, &st) != 0;
}

/**
 * Write the process working directory into out, NUL-terminated.
 *
 * @param out      caller-owned buffer
 * @param out_cap  capacity of out in bytes; DIR_PATH_MAX is always enough
 * @return length written, 0 when the directory cannot be determined (it was removed, or it
 *         does not fit)
 *
 * ADDED 2026-08-22 AS A FOURTH FUNCTION BEYOND THE THREE THAT WERE ORDERED, on a
 * measurement from the consumer side: after core/ase-log/src/log_files.cpp moved onto this
 * module, exactly ONE filesystem call was left standing in it - the working-directory read
 * in the fallback of get_project_root. Ten lines here take that file from one finding to
 * none, and no other header in the module could host them without growing a dependency.
 *
 * It lives on the buffer side rather than in path.hpp because that is what it is: a fixed
 * bound and a caller-owned target, the same shape as list_dir above. path.hpp works in
 * strings and holds no buffer at all.
 */
[[nodiscard]] inline uint32_t current_dir(char* out, uint32_t out_cap) {
    if (out == nullptr || out_cap == 0u) return 0u;
    if (::getcwd(out, out_cap) == nullptr) {
        out[0] = '\0';  // never hand back the caller's uninitialised bytes as a path
        return 0u;
    }
    return detail::bounded_length(out, out_cap);
}

}  // namespace ase::fileio
