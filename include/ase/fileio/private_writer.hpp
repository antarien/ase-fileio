#pragma once

/**
 * @file        private_writer.hpp
 * @brief       Owner-only file writing - born 0600, replaced whole, no exceptions
 * @description The write form for a file nobody but its owner may read: a credential the
 *              operator's CLI keeps between two invocations. text_writer.hpp is the wrong
 *              form for it on two counts, and both stand in that header itself:
 *
 *                mode     FILE_CREATE_MODE is 0644. Narrowing the rights after the write
 *                         leaves a window in which the content stands with group and world
 *                         read, and a file that already existed keeps its old rights while
 *                         the new content goes in.
 *                replace  O_TRUNC empties the file before the first byte of the new content
 *                         arrives. A failure mid-write leaves a half file behind that the
 *                         next reader takes for a whole one.
 *
 *              So the content goes to a sibling path created with O_CREAT | O_EXCL and mode
 *              0600 - the rights are fixed at the moment the inode is born - and is renamed
 *              over the target once every byte reached it and close() succeeded. rename()
 *              replaces the directory entry atomically: a reader sees the previous file or
 *              the complete new one, never a state in between, and the new file never had
 *              wider rights than its owner's. O_EXCL also refuses a symbolic link planted at
 *              the sibling path instead of following it.
 *
 *              ONE CALL SITE, measured 2026-10-06: tools/ase-cli, vault_write_cached_token,
 *              the Vault admin token that `/login` leaves for the commands after it. The
 *              read side is text_reader.hpp; such a file reads like any other.
 *
 * @module      ase-fileio
 * @layer       0 (Foundation)
 * @category    structure/filesystem/io
 * @created     2026-10-06
 * @modified    2026-10-06
 * @version     1.0.0
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <string>

namespace ase::fileio {

/// Permission bits of a file this header creates: owner read+write, nothing for group and
/// world. The process umask can only narrow them further.
constexpr unsigned int PRIVATE_FILE_MODE = 0600u;

/// Suffix of the sibling path the content is staged under until the rename.
constexpr const char* PRIVATE_STAGE_SUFFIX = ".new";

/**
 * Replace path with content so that the file is never readable by anyone but its owner and
 * never seen half written. The containing directory must already exist.
 *
 * Returns true when the content stands complete at path, false on any error (directory
 * missing, permission denied, disk full, a foreign entry at the staging path). On false the
 * previous file at path is untouched and the staging path is removed again.
 *
 * A stale staging file from an interrupted earlier call is unlinked first; O_EXCL would
 * refuse every later call otherwise. unlink() removes a symbolic link itself, never its
 * target, and should one be planted again between the unlink and the open, the open fails
 * and nothing is written.
 */
inline bool write_private_text(const std::string& path, const std::string& content) {
    const std::string staged = path + PRIVATE_STAGE_SUFFIX;
    ::unlink(staged.c_str());
    const int fd = ::open(staged.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
                          static_cast<mode_t>(PRIVATE_FILE_MODE));
    if (fd < 0) return false;

    size_t written = 0u;
    bool complete = true;
    while (written < content.size()) {
        const ssize_t n = ::write(fd, content.data() + written, content.size() - written);
        if (n > 0) {
            written += static_cast<size_t>(n);
            continue;
        }
        // Same reading as write_text: a short write is handled by the loop, EINTR is a
        // signal and not a failure, and a zero return for a non-empty rest ends the attempt.
        if (n < 0 && errno == EINTR) continue;
        complete = false;
        break;
    }

    // The result of close() belongs to the answer, as in write_text: an error that surfaces
    // only there means the content never landed, and renaming it over the target would
    // replace a sound file with a broken one.
    const bool closed = (::close(fd) == 0);
    if (!complete || !closed || std::rename(staged.c_str(), path.c_str()) != 0) {
        ::unlink(staged.c_str());
        return false;
    }
    return true;
}

}  // namespace ase::fileio
