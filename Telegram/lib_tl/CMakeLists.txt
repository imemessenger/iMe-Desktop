# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

add_library(lib_tl OBJECT)
add_library(desktop-app::lib_tl ALIAS lib_tl)
init_target(lib_tl)

get_filename_component(src_loc . REALPATH)

nice_target_sources(lib_tl ${src_loc}
PRIVATE
    tl/tl_basic_types.cpp
    tl/tl_basic_types.h
    tl/tl_boxed.h
    tl/tl_type_owner.h

    tl/generate_tl.py
)

target_include_directories(lib_tl
PUBLIC
    ${src_loc}
)

target_link_libraries(lib_tl
PUBLIC
    desktop-app::lib_base
)
