#pragma once

/**
 * @file        text_reader.hpp
 * @brief       Plain text file reading - lines or full-blob, no exceptions
 * @description Tiny ASE-native wrapper around std::ifstream for non-ECS callers
 *              (clients, build helpers, manifest loaders). Errors are signalled by
 *              empty results, never by exceptions. ECS state persistence belongs in
 *              ase-persist, not here.
 *
 * @module      ase-fileio
 * @layer       0 (Foundation)
 * @created     2026-04-13
 * @modified    2026-04-13
 * @version     1.0.0
 */

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace ase::fileio {

/**
 * Returns true if a regular file exists at path. Cheap, no I/O beyond stat.
 * Symlinks are followed. Directories return false.
 */
inline bool file_exists(std::string_view path) {
    std::error_code ec;
    std::filesystem::path p((std::string(path)));
    return std::filesystem::is_regular_file(p, ec);
}

/**
 * Read the entire file into a single string. Returns an empty string on any error
 * (file not found, permission denied, read error). Does not throw.
 */
inline std::string read_text(std::string_view path) {
    std::ifstream in((std::string(path)), std::ios::in | std::ios::binary);
    if (!in.is_open()) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/**
 * Read the file line by line into a vector. Trailing CR characters are stripped
 * (handles both LF and CRLF line endings). Returns an empty vector on any error.
 * The terminating newline of the last line is not preserved.
 */
inline std::vector<std::string> read_lines(std::string_view path) {
    std::vector<std::string> lines;
    std::ifstream in((std::string(path)));
    if (!in.is_open()) return lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
    }
    return lines;
}

}  // namespace ase::fileio
