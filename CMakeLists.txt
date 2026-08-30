cmake_minimum_required(VERSION 3.16)

set(DFZH_VERSION "v0.8.4")

project(dfzh)

# ============================================================
# 1. Architecture
# ============================================================

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_SUBDIR x64)
else()
    set(ARCH_SUBDIR x86)
endif()


# ============================================================
# 2. SDL2
#
# DFHack provides SDL2.
# ============================================================

if(NOT EXISTS "${SDL2_INCLUDE_DIRS}/SDL.h")
    message(FATAL_ERROR
        "SDL2 headers not found at: ${SDL2_INCLUDE_DIRS}"
    )
endif()


# ============================================================
# 3. vcpkg
#
# Dependencies are installed through:
#
# 3rdParty/vcpkg.json
#
# and therefore appear under:
#
# 3rdParty/vcpkg_installed/x64-windows
#
# ============================================================

if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows")
endif()

set(VCPKG_INSTALLED
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/vcpkg_installed/${VCPKG_TARGET_TRIPLET}"
)

message(STATUS "DFZH vcpkg installed directory:")
message(STATUS "  ${VCPKG_INSTALLED}")


# ============================================================
# 4. Verify vcpkg installation
# ============================================================

if(NOT EXISTS "${VCPKG_INSTALLED}")
    message(FATAL_ERROR
        "DFZH vcpkg installation not found:\n"
        "  ${VCPKG_INSTALLED}\n"
        "\n"
        "Please run vcpkg manifest installation before configuring CMake."
    )
endif()


# ============================================================
# 5. Detours
# ============================================================

find_path(
    DETOURS_INCLUDE_DIRS
    "detours/detours.h"
    PATHS
        "${VCPKG_INSTALLED}/include"
    NO_DEFAULT_PATH
)

find_library(
    DETOURS_LIBRARY
    detours
    PATHS
        "${VCPKG_INSTALLED}/lib"
    NO_DEFAULT_PATH
)

if(NOT DETOURS_INCLUDE_DIRS)
    message(FATAL_ERROR
        "Detours headers not found in:\n"
        "  ${VCPKG_INSTALLED}/include"
    )
endif()

if(NOT DETOURS_LIBRARY)
    message(FATAL_ERROR
        "Detours library not found in:\n"
        "  ${VCPKG_INSTALLED}/lib"
    )
endif()

message(STATUS "Detours include directory:")
message(STATUS "  ${DETOURS_INCLUDE_DIRS}")

message(STATUS "Detours library:")
message(STATUS "  ${DETOURS_LIBRARY}")


# ============================================================
# 6. spdlog
# ============================================================

find_path(
    SPDLOG_INCLUDE_DIRS
    "spdlog/spdlog.h"
    PATHS
        "${VCPKG_INSTALLED}/include"
    NO_DEFAULT_PATH
)

if(NOT SPDLOG_INCLUDE_DIRS)
    message(FATAL_ERROR
        "spdlog headers not found in:\n"
        "  ${VCPKG_INSTALLED}/include"
    )
endif()

message(STATUS "spdlog include directory:")
message(STATUS "  ${SPDLOG_INCLUDE_DIRS}")


# ============================================================
# 7. toml++
# ============================================================

find_path(
    TOMLPLUSPLUS_INCLUDE_DIRS
    "toml++/toml.hpp"
    PATHS
        "${VCPKG_INSTALLED}/include"
    NO_DEFAULT_PATH
)

if(NOT TOMLPLUSPLUS_INCLUDE_DIRS)
    message(FATAL_ERROR
        "toml++ headers not found in:\n"
        "  ${VCPKG_INSTALLED}/include"
    )
endif()

message(STATUS "toml++ include directory:")
message(STATUS "  ${TOMLPLUSPLUS_INCLUDE_DIRS}")


# ============================================================
# 8. SDL2_ttf
# ============================================================

option(
    USE_SYSTEM_SDL2_TTF
    "Use system-installed SDL2_ttf"
    OFF
)

if(USE_SYSTEM_SDL2_TTF)

    message(STATUS "Using system SDL2_ttf")

else()

    set(SDL2_TTF_VERSION 2.24.0)

    set(
        SDL2_TTF_ZIP_MD5
        64f9ef877af440dbc11c840732a60104
    )

    set(
        SDL2_TTF_ZIP_FILE
        SDL2_ttf-devel-${SDL2_TTF_VERSION}-VC.zip
    )

    set(
        SDL2_TTF_SOURCE_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/SDL2_ttf
    )

    set(
        SDL2_TTF_ZIP_PATH
        ${SDL2_TTF_SOURCE_DIR}/${SDL2_TTF_ZIP_FILE}
    )

    set(
        SDL2_TTF_EXTRACT_DIR
        ${SDL2_TTF_SOURCE_DIR}/SDL2_ttf-${SDL2_TTF_VERSION}
    )

    file(MAKE_DIRECTORY
        "${SDL2_TTF_SOURCE_DIR}"
    )

    if(NOT EXISTS "${SDL2_TTF_ZIP_PATH}")

        message(STATUS
            "Downloading SDL2_ttf ${SDL2_TTF_VERSION}"
        )

        download_file(
            "https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL2_TTF_VERSION}/${SDL2_TTF_ZIP_FILE}"
            "${SDL2_TTF_ZIP_PATH}"
            "${SDL2_TTF_ZIP_MD5}"
        )

    endif()

    if(NOT EXISTS "${SDL2_TTF_EXTRACT_DIR}")

        message(STATUS
            "Extracting SDL2_ttf"
        )

        file(
            ARCHIVE_EXTRACT
            INPUT "${SDL2_TTF_ZIP_PATH}"
            DESTINATION "${SDL2_TTF_SOURCE_DIR}"
        )

    endif()

    set(
        SDL2_TTF_INCLUDE_DIR
        ${SDL2_TTF_EXTRACT_DIR}/include
    )

    set(
        SDL2_TTF_DLL_PATH
        ${SDL2_TTF_EXTRACT_DIR}/lib/${ARCH_SUBDIR}/SDL2_ttf.dll
    )

endif()


if(NOT EXISTS "${SDL2_TTF_INCLUDE_DIR}")

    message(FATAL_ERROR
        "SDL2_ttf include directory not found:\n"
        "  ${SDL2_TTF_INCLUDE_DIR}"
    )

endif()


# ============================================================
# 9. DFZH source files
# ============================================================

set(PROJECT_SRCS

    dfzh/dfzh.cpp
    dfzh/hooks.cpp
    dfzh/sdl2_hooks.cpp
    dfzh/dict_manager.cpp
    dfzh/rulesets_manager.cpp
    dfzh/sentence_detector.cpp
    dfzh/screen_manager.cpp
    dfzh/ttf_manager.cpp
    dfzh/logger.cpp
    dfzh/config.cpp
    dfzh/ai_translator.cpp

)


# ============================================================
# 10. cpp-httplib
# ============================================================

set(
    CPP_HTTPLIB_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/cpp-httplib"
)

if(NOT EXISTS
    "${CPP_HTTPLIB_INCLUDE_DIR}/httplib.h"
)

    message(FATAL_ERROR
        "cpp-httplib not found at:\n"
        "  ${CPP_HTTPLIB_INCLUDE_DIR}/httplib.h"
    )

endif()


# ============================================================
# 11. Create DFHack plugin
# ============================================================

dfhack_plugin(
    dfzh
    ${PROJECT_SRCS}

    LINK_LIBRARIES
        ${DETOURS_LIBRARY}
        ws2_32

    COMPILE_FLAGS_MSVC
        "/utf-8"
)


# ============================================================
# 12. Include directories
# ============================================================

target_include_directories(
    dfzh
    PRIVATE

        ${DETOURS_INCLUDE_DIRS}

        ${SPDLOG_INCLUDE_DIRS}

        ${TOMLPLUSPLUS_INCLUDE_DIRS}

        ${SDL2_INCLUDE_DIRS}

        ${SDL2_TTF_INCLUDE_DIR}

        ${CPP_HTTPLIB_INCLUDE_DIR}

        ${CMAKE_CURRENT_SOURCE_DIR}/dfzh
)


# ============================================================
# 13. Compile definitions
# ============================================================

target_compile_definitions(
    dfzh
    PRIVATE

        FMT_HEADER_ONLY

        DFZH_VERSION="${DFZH_VERSION}"
)


# ============================================================
# 14. Install DFZH data
# ============================================================

install(
    DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/data/

    DESTINATION
        "${DFHACK_DATA_DESTINATION}/data/dfzh"
)


# ============================================================
# 15. Install SDL2_ttf.dll
# ============================================================

if(EXISTS "${SDL2_TTF_DLL_PATH}")

    install(
        FILES
            "${SDL2_TTF_DLL_PATH}"

        DESTINATION
            "${DFHACK_BINARY_DESTINATION}"

        COMPONENT
            Runtime
    )

else()

    message(
        WARNING
        "SDL2_ttf.dll not found at ${SDL2_TTF_DLL_PATH}"
    )

endif()


# ============================================================
# 16. Final information
# ============================================================

message(STATUS "")
message(STATUS "========================================")
message(STATUS "DFZH configuration complete")
message(STATUS "========================================")
message(STATUS "DFZH version: ${DFZH_VERSION}")
message(STATUS "Architecture: ${ARCH_SUBDIR}")
message(STATUS "vcpkg triplet: ${VCPKG_TARGET_TRIPLET}")
message(STATUS "vcpkg installed: ${VCPKG_INSTALLED}")
message(STATUS "Detours: ${DETOURS_LIBRARY}")
message(STATUS "spdlog: ${SPDLOG_INCLUDE_DIRS}")
message(STATUS "toml++: ${TOMLPLUSPLUS_INCLUDE_DIRS}")
message(STATUS "SDL2_ttf: ${SDL2_TTF_INCLUDE_DIR}")
message(STATUS "cpp-httplib: ${CPP_HTTPLIB_INCLUDE_DIR}")
message(STATUS "========================================")
message(STATUS "")
