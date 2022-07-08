# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

add_library(lib_qr OBJECT)
add_library(desktop-app::lib_qr ALIAS lib_qr)
init_target(lib_qr)

get_filename_component(src_loc . REALPATH)

nice_target_sources(lib_qr ${src_loc}
PRIVATE
    qr/qr_generate.cpp
    qr/qr_generate.h
)

target_include_directories(lib_qr
PUBLIC
    ${src_loc}
)

target_link_libraries(lib_qr
PUBLIC
    desktop-app::lib_base
    desktop-app::external_qr_code_generator
)
