# ase-fileio

[![Layer](https://img.shields.io/badge/Layer-0%20Foundation-blue.svg)]()
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)]()
[![Header Only](https://img.shields.io/badge/Header-Only-green.svg)]()

> Plain text file reading for non-ECS code

Part of [ASE - Antares Simulation Engine](../../..)

## Why this module exists

ECS systems persist their state through `ase-persist` and MongoDB — never through plain text files. But the codebase has many non-ECS callers that legitimately need to read text files: client tools parsing `.gitmodules`, build helpers reading `VERSION` registries, configuration loaders consuming module manifests, the explorer scanning project metadata, and so on. These callers used to reach for `std::ifstream` directly, which the validator forbids in all non-foundation code because `<fstream>` brings exception-throwing C++ I/O and a deprecated stream model. `ase-fileio` is the foundation-layer escape hatch: it wraps `std::ifstream` once, in one whitelisted module, and exposes a tiny ASE-native API that returns plain `std::vector<std::string>` lines or a single `std::string` blob. Callers in clients, modules, and plugins use this instead of `<fstream>`, and the validator stays happy because no forbidden include leaves the foundation layer.

## Public API

```cpp
#include <ase/fileio/text_reader.hpp>

// Read entire file as one string (returns empty string on error).
std::string blob = ase::fileio::read_text("/path/to/file.txt");

// Read line by line (returns empty vector on error).
std::vector<std::string> lines = ase::fileio::read_lines("/path/to/file.txt");

// Check existence without opening (cheap, no exceptions).
if (ase::fileio::file_exists("/path/to/file.txt")) {
    // ...
}
```

All functions are noexcept-friendly: errors are signalled by an empty result, never by exceptions. There is no write API in this module — writes belong to `ase-persist` (ECS state) or to module-specific persistence layers.

## Layer

Layer 0 (Foundation). No ASE dependencies. Header-only library — `target_link_libraries(my_target ... ase::fileio)`. Internally uses `std::ifstream` (allowed because `/foundation/` is in the validator whitelist).

## Build

Built as part of the ASE root build. Standalone:

```bash
cd foundation/ase-fileio
cmake -B build -G Ninja
ninja -C build
```
