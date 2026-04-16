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
Plain text reading for non-ECS code. Internally wraps std::ifstream.
]]
add_library(${PROJECT_NAME} INTERFACE
    include/ase/fileio/text_reader.hpp
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
