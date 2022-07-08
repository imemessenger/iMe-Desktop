# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

add_library(lib_waylandshells_xdg_shell STATIC)
add_library(desktop-app::lib_waylandshells::xdg_shell ALIAS lib_waylandshells_xdg_shell)
init_target(lib_waylandshells_xdg_shell)

get_filename_component(src_loc . REALPATH)
set_target_properties(lib_waylandshells_xdg_shell PROPERTIES AUTOMOC ON)

if (DESKTOP_APP_QT6)
    qt6_generate_wayland_protocol_client_sources(lib_waylandshells_xdg_shell
    FILES
        ${third_party_loc}/wayland-protocols/stable/xdg-shell/xdg-shell.xml
        ${third_party_loc}/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
        ${third_party_loc}/wayland-protocols/staging/xdg-activation/xdg-activation-v1.xml
    )
else()
    message(FATAL_ERROR "This piece of cmake code is not ported to Qt 5")
endif()

nice_target_sources(lib_waylandshells_xdg_shell ${src_loc}
PRIVATE
    main.cpp
    qwaylandxdgactivationv1.cpp
    qwaylandxdgactivationv1_p.h
    qwaylandxdgdecorationv1.cpp
    qwaylandxdgdecorationv1_p.h
    qwaylandxdgshell.cpp
    qwaylandxdgshell_p.h
    qwaylandxdgshellintegration.cpp
    qwaylandxdgshellintegration_p.h
)

target_include_directories(lib_waylandshells_xdg_shell
PRIVATE
    ${src_loc}
)

target_compile_options(lib_waylandshells_xdg_shell
PRIVATE
    -UQT_NO_KEYWORDS
)

target_compile_definitions(lib_waylandshells_xdg_shell
PRIVATE
    QT_STATICPLUGIN
)

target_link_libraries(lib_waylandshells_xdg_shell
PRIVATE
    desktop-app::external_qt
    desktop-app::external_wayland_client
)
