# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

add_library(lib_spellcheck STATIC)
add_library(desktop-app::lib_spellcheck ALIAS lib_spellcheck)
init_target(lib_spellcheck)

get_filename_component(src_loc . REALPATH)

set(use_enchant 0)
if (LINUX AND DESKTOP_APP_USE_ENCHANT)
    set(use_enchant 1)
endif()

set(system_spellchecker 0)
if ((APPLE OR WIN32 OR use_enchant) AND NOT DESKTOP_APP_USE_HUNSPELL_ONLY)
    set(system_spellchecker 1)
endif()

target_precompile_headers(lib_spellcheck PRIVATE ${src_loc}/spellcheck/spellcheck_pch.h)
nice_target_sources(lib_spellcheck ${src_loc}
PRIVATE
    spellcheck/platform/platform_spellcheck.h
    spellcheck/spellcheck_utils.cpp
    spellcheck/spellcheck_utils.h
    spellcheck/spellcheck_types.h
    spellcheck/spelling_highlighter.cpp
    spellcheck/spelling_highlighter.h
    spellcheck/spelling_highlighter_helper.cpp
    spellcheck/spelling_highlighter_helper.h
    spellcheck/spellcheck_value.cpp
    spellcheck/spellcheck_value.h

    spellcheck/spellcheck_pch.h
)

if (system_spellchecker)
    nice_target_sources(lib_spellcheck ${src_loc}
    PRIVATE
        spellcheck/platform/linux/linux_enchant.cpp
        spellcheck/platform/linux/linux_enchant.h
        spellcheck/platform/linux/spellcheck_linux.cpp
        spellcheck/platform/linux/spellcheck_linux.h
        spellcheck/platform/mac/spellcheck_mac.h
        spellcheck/platform/mac/spellcheck_mac.mm
        spellcheck/platform/win/spellcheck_win.cpp
        spellcheck/platform/win/spellcheck_win.h
    )
else()
    nice_target_sources(lib_spellcheck ${src_loc}
    PRIVATE
        spellcheck/third_party/spellcheck_hunspell.cpp
        spellcheck/third_party/spellcheck_hunspell.h
    )
endif()

# We should support both types of spellchecker for Windows.
if (NOT system_spellchecker OR WIN32)
    nice_target_sources(lib_spellcheck ${src_loc}
    PRIVATE
        spellcheck/third_party/hunspell_controller.cpp
        spellcheck/third_party/hunspell_controller.h
    )
    target_link_libraries(lib_spellcheck PRIVATE desktop-app::external_hunspell)
endif()

target_include_directories(lib_spellcheck
PUBLIC
    ${src_loc}
)

target_link_libraries(lib_spellcheck
PRIVATE
    desktop-app::lib_ui
    desktop-app::lib_base
    desktop-app::lib_rpl
    desktop-app::lib_crl
    desktop-app::external_qt
    desktop-app::external_ranges
    desktop-app::external_gsl
)

if (LINUX AND use_enchant)
    find_package(PkgConfig REQUIRED)

    pkg_search_module(ENCHANT REQUIRED enchant-2 enchant)
    target_include_directories(lib_spellcheck SYSTEM PRIVATE ${ENCHANT_INCLUDE_DIRS})
endif()
