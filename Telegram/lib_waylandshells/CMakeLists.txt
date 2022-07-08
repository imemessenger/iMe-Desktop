# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

add_library(lib_waylandshells OBJECT)
add_library(desktop-app::lib_waylandshells ALIAS lib_waylandshells)
init_target(lib_waylandshells)

add_subdirectory(waylandshells/xdg-shell)
get_filename_component(src_loc . REALPATH)

nice_target_sources(lib_waylandshells ${src_loc}
PRIVATE
    waylandshells/xdg_shell.cpp
    waylandshells/xdg_shell.h
)

target_include_directories(lib_waylandshells
PUBLIC
    ${src_loc}
)

target_link_libraries(lib_waylandshells
PUBLIC
    desktop-app::lib_waylandshells::xdg_shell
    desktop-app::external_qt
)
