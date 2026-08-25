#pragma once

/**
 * @file        text_reader.hpp
 * @brief       Plain text file reading - lines or full-blob, no exceptions
 * @description ASE-native file reading for non-ECS callers (clients, build helpers,
 *              manifest loaders). Errors are signalled by empty results, never by
 *              exceptions. ECS state persistence belongs in ase-persist, not here.
 *
 *              BUILT ON POSIX SINCE 2026-08-22, and the rewrite is the point of the file
 *              rather than a detail of it. Until then this header was a wrapper around
 *              the standard stream and path facilities - around exactly what the
 *              forbidden-type rules name this module as the answer TO. A module that
 *              exists so its callers need not say `ifstream` cannot be built by saying
 *              it either; that is a coupling moved, not dissolved, and the same mistake
 *              the ring buffer made with its optional return until the same day.
 *
 *              The old header argued the opposite in its own text: "The std:: findings on
 *              this file are the module, not a defect - it exists in order to be the one
 *              place that says ifstream." THAT ARGUMENT IS WITHDRAWN, and by measurement,
 *              not by taste: open/read/stat do the same work, cost this module nothing,
 *              and take the finding count of the file from 8 to 0. A facade is only
 *              entitled to the exemption it cannot avoid.
 *
 *              PARAMETERS ARE `const std::string&`, NOT `const char*` PLUS LENGTH, and
 *              this deviates from the wording of STD_STRING_VIEW_FORBIDDEN's suggestion.
 *              The reason is measured, and it is reported to the stakeholder rather than
 *              settled here: every consumer of these functions holds a std::string
 *              already - log_files.cpp passes one straight through, and both read_lines
 *              call sites in submodule.cpp pass `x.str()`. The return types are
 *              `std::string` and a vector of them, and
 *              the rules that forbid those do not reach this file (they carry a file
 *              filter of component/tag/system/types). A function that RETURNS a string
 *              but refuses to ACCEPT one is not stricter, only stranger. The byte-facing
 *              half of this module - directory.hpp - does take (pointer, length), because
 *              ITS consumer holds char buffers. Each side matches its measured caller.
 *
 *              WHAT THIS MODULE IS, measured 2026-08-22:
 *                reads       file_exists, read_text, read_lines (this file)
 *                writes      write_text (text_writer.hpp)
 *                directories list_dir, remove_file (directory.hpp)
 *                paths       path_exists, is_directory, parent_of, create_directories
 *                            (path.hpp)
 *
 * @module      ase-fileio
 * @layer       0 (Foundation)
 * @category    structure/filesystem/io
 * @created     2026-04-13
 * @modified    2026-08-22
 * @version     2.0.0
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <string>
#include <vector>

namespace ase::fileio {

/// Read granularity for files whose size stat() will not tell us (pipes, /proc entries).
/// Regular files are reserved up front from their stat size and take one pass.
constexpr unsigned int READ_CHUNK_BYTES = 8192u;

/**
 * Returns true if a REGULAR FILE exists at path. Symlinks are followed, directories
 * return false.
 *
 * THE NAME CARRIES THE NARROWER OF TWO QUESTIONS, DELIBERATELY. `path_exists` in path.hpp
 * answers the other one - true for any entry, directories included. Both return bool, and
 * no compiler will ever tell a caller it picked the wrong one, which is precisely why
 * they have different names and live one lookup apart. Callers migrating off a
 * filesystem-style `exists` want `path_exists`; callers guarding a read want this.
 */
inline bool file_exists(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

/**
 * Read the entire file into a single string. Returns an empty string on any error (file
 * not found, permission denied, read error). Does not throw.
 *
 * An empty return does not distinguish "empty file" from "unreadable file" - it did not
 * before the POSIX rewrite either, and the callers that care ask file_exists first.
 *
 * Bytes are passed through unchanged: no line-ending translation, no encoding guess.
 */
inline std::string read_text(const std::string& path) {
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) return {};

    std::string out;

    // A regular file tells us its size, so the string is sized once instead of growing.
    // Anything else (pipe, /proc) reports 0 and is simply read until the end.
    struct stat st;
    if (::fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
        out.reserve(static_cast<size_t>(st.st_size));
    }

    char buffer[READ_CHUNK_BYTES];
    for (;;) {
        const ssize_t n = ::read(fd, buffer, READ_CHUNK_BYTES);
        if (n > 0) {
            out.append(buffer, static_cast<size_t>(n));
            continue;
        }
        if (n == 0) break;             // end of file
        if (errno == EINTR) continue;  // interrupted by a signal, not an error
        ::close(fd);
        return {};                     // real read error: empty result, as promised above
    }

    ::close(fd);
    return out;
}

/**
 * Read the file line by line. Trailing CR characters are stripped (so LF and CRLF both
 * work). Returns an empty vector on any error. The terminating newline of the last line
 * is not preserved, and a file ending in a newline does NOT yield a trailing empty line -
 * that matches what the stream-based version did and what its two callers expect.
 */
inline std::vector<std::string> read_lines(const std::string& path) {
    std::vector<std::string> lines;

    const std::string content = read_text(path);
    size_t start = 0u;
    while (start < content.size()) {
        const size_t newline = content.find('\n', start);
        const size_t end     = (newline == std::string::npos) ? content.size() : newline;

        size_t trimmed = end;
        if (trimmed > start && content[trimmed - 1u] == '\r') --trimmed;
        lines.emplace_back(content, start, trimmed - start);

        if (newline == std::string::npos) break;
        start = newline + 1u;
    }

    return lines;
}

}  // namespace ase::fileio
