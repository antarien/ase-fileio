cmake_minimum_required(VERSION 3.20)

#[[
ASE FOUNDATION BUILD CONFIGURATION

@file        CMakeLists.txt
@brief       Build configuration for ase-fileio
@description Plain text file reading for non-ECS callers.
             Header-only library wrapping std::ifstream behind ASE-native API.

@module      ase-fileio
@layer       0 (Foundation)
@created     2026-04-13
@modified    2026-04-13
@version     00.00.01.00001 [seed]
]]

# Parse local VERSION (SSOT) if not building from root
if(NOT DEFINED ASE_FILEIO_VERSION)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
        message(FATAL_ERROR "VERSION file not found: ${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    endif()
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _VERSION_LINES)
    foreach(_LINE ${_VERSION_LINES})
        if(_LINE MATCHES "^ASE_FILEIO=(.*)$")
            set(ASE_FILEIO_VERSION "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(NOT DEFINED ASE_FILEIO_VERSION)
        message(FATAL_ERROR "ASE_FILEIO version not found in VERSION file")
    endif()
endif()

project(ase-fileio VERSION ${ASE_FILEIO_VERSION} LANGUAGES CXX)

# Generate version.hpp from template (SSOT)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ase/fileio/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/ase/fileio/version.hpp"
    @ONLY
)

# ==============================================================================
# BUILD CONFIGURATION
# ==============================================================================

message(STATUS "Configuring ${PROJECT_NAME} v${PROJECT_VERSION}")

#[[
HEADER-ONLY LIBRARY
Dateizugriff fuer Nicht-ECS-Code, seit dem 2026-08-22 durchgaengig auf POSIX gebaut
(open/read/write, opendir/readdir, stat/mkdir/unlink). Vorher lagen Lese- und Schreibseite
auf genau den Bibliotheksteilen, als deren Ersatz dieses Modul in den Regeltexten steht —
eine Kopplung, die ein Wrapper verdeckt statt sie aufzuloesen.
]]
add_library(${PROJECT_NAME} INTERFACE
    #[[ Leseseite. file_exists prueft eine REGULAERE Datei; die weitere Frage (jeder
        Eintrag) heisst path_exists und steht in path.hpp. ]]
    include/ase/fileio/text_reader.hpp
    #[[ Die Schreibseite, 2026-08-20. Gemessen vor dem Bau: acht Ausgabestrom-Stellen im
        Baum (codegen 4, convert 3, log 1), ALLE acht Text mit Truncation — kein app, kein
        binary. Deshalb genau eine Funktion und keine Formauswahl. ]]
    include/ase/fileio/text_writer.hpp
    #[[ Verzeichnisseite, 2026-08-22. Form von ihrem Verbraucher erhoben
        (core/ase-log, prune_log_dir): Name, Aenderungszeit und is_regular je Eintrag,
        flach, ohne Filter — und ein idempotentes remove_file. ]]
    include/ase/fileio/directory.hpp
    #[[ Pfadseite, 2026-08-22. Die vier Funktionen, die der Baum wirklich ruft
        (path_exists 15, create_directories 7, parent_of 2, is_directory 1). Die vier
        ohne jeden Rufer sind bewusst nicht mitgewandert. ]]
    include/ase/fileio/path.hpp
)
add_library(ase::fileio ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)

#[[ VERBOTEN: No dependencies allowed in Layer 0! ]]

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

#[[ Die Existenzpruefung ist notwendig, nicht vorsichtig: dieses Modul hat noch kein
    tests/-Verzeichnis, und ein add_subdirectory auf ein fehlendes Verzeichnis bricht die
    Konfiguration des GANZEN Baums. Sobald ein Test entsteht, greift der Zweig von selbst. ]]
if(ASE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()
