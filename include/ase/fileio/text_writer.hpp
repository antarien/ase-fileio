#pragma once

/**
 * @file        text_writer.hpp
 * @brief       Plain text file writing - whole-file, truncating, no exceptions
 * @description The other half of text_reader.hpp. Same shape, same promises: ASE-native
 *              file writing for non-ECS callers, errors signalled by a false return
 *              rather than an exception. ECS state persistence belongs elsewhere.
 *
 *              WHY THERE IS EXACTLY ONE WRITE FORM, measured 2026-08-20 across every
 *              output-stream construction in the tree before this file was written:
 *
 *                core/ase-codegen    4  interface_versioning.hpp:327, file_writer.hpp:102
 *                                       and :161, typescript_generator.cpp:364
 *                core/ase-convert    3  keyword_search.cpp:392 and :438,
 *                                       markdown_writer.cpp:147
 *                core/ase-log        1  log_files.cpp:150
 *                                    -
 *                                    8 call sites, and ALL EIGHT are text mode with
 *                                      truncation. Not one appends, not one is binary.
 *                                      The single explicit flag in the tree was `trunc`,
 *                                      which is the default anyway.
 *
 *              So this header offers append to nobody and binary to nobody. A write side
 *              that can do more than the tree asks for is a second truth: the day someone
 *              needs appending, they add it here WITH their call site, and the count above
 *              tells the next reader whether that day has come.
 *
 *              BUILT ON POSIX SINCE 2026-08-22, two days after the file was first written,
 *              and the correction is worth recording rather than hiding. The first version
 *              of this header argued: "it exists in order to be the one place that says
 *              ofstream, so that its callers do not have to - the std:: findings on this
 *              file are the module, not a defect." THAT ARGUMENT IS WITHDRAWN, here and in
 *              text_reader.hpp, on the same measurement: open/write/close do the same work
 *              with no facility this module is meant to replace, so the exemption was
 *              never needed. A facade is entitled only to the exemption it cannot avoid,
 *              and this one could.
 *
 *              CALLERS BUFFER, THIS FUNCTION WRITES. The eight sites above stream into
 *              their file incrementally (`file << a << b << "\n"` inside loops). Moving
 *              them here means assembling the content first and handing over one string.
 *              That is the deliberate trade: a whole-file call cannot leave a half-written
 *              file behind on an early return, which incremental streaming does.
 *
 * @module      ase-fileio
 * @layer       0 (Foundation)
 * @category    structure/filesystem/io
 * @created     2026-08-20
 * @modified    2026-08-22
 * @version     2.0.0
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <string>

namespace ase::fileio {

/// Permission bits for a file this module creates: owner read+write, group and world read.
/// The process umask still applies on top, so a restrictive umask stays in force.
constexpr unsigned int FILE_CREATE_MODE = 0644u;

/**
 * Write content to path, replacing whatever was there. Creates the file if it does not
 * exist; the containing directory must already exist (path.hpp has create_directories).
 *
 * Returns true when the whole content reached the file and it closed cleanly, false on
 * any error (permission denied, directory missing, disk full, path is a directory).
 * Does not throw.
 *
 * A false return may leave a partially written file: the failure can happen mid-write,
 * and no filesystem gives an atomic replace for free. Callers who need all-or-nothing
 * write to a temporary path and rename.
 *
 * THE RESULT OF close() IS PART OF THE ANSWER, not a formality. A write can be accepted
 * into the page cache and fail later; on a network filesystem the error surfaces at close
 * and nowhere else. Ignoring it would report success for content that never landed.
 */
inline bool write_text(const std::string& path, const std::string& content) {
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC,
                          static_cast<mode_t>(FILE_CREATE_MODE));
    if (fd < 0) return false;

    size_t written = 0u;
    while (written < content.size()) {
        const ssize_t n = ::write(fd, content.data() + written, content.size() - written);
        if (n > 0) {
            written += static_cast<size_t>(n);
            continue;
        }
        // A short write is normal and already handled by the loop; only a negative return
        // is an error, and EINTR among those is a signal, not a failure.
        if (n < 0 && errno == EINTR) continue;
        ::close(fd);
        return false;
    }

    return ::close(fd) == 0;
}

}  // namespace ase::fileio
