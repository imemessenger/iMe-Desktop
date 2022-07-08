# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

add_library(lib_storage OBJECT)
add_library(desktop-app::lib_storage ALIAS lib_storage)
init_target(lib_storage)

get_filename_component(src_loc . REALPATH)

target_precompile_headers(lib_storage PRIVATE ${src_loc}/storage/storage_pch.h)
nice_target_sources(lib_storage ${src_loc}
PRIVATE
    storage/storage_clear_legacy.cpp
    storage/storage_clear_legacy_posix.cpp
    storage/storage_clear_legacy_win.cpp
    storage/storage_clear_legacy.h
    storage/storage_databases.cpp
    storage/storage_databases.h
    storage/storage_encrypted_file.cpp
    storage/storage_encrypted_file.h
    storage/storage_encryption.cpp
    storage/storage_encryption.h
    storage/cache/storage_cache_binlog_reader.cpp
    storage/cache/storage_cache_binlog_reader.h
    storage/cache/storage_cache_cleaner.cpp
    storage/cache/storage_cache_cleaner.h
    storage/cache/storage_cache_compactor.cpp
    storage/cache/storage_cache_compactor.h
    storage/cache/storage_cache_database.cpp
    storage/cache/storage_cache_database.h
    storage/cache/storage_cache_database_object.cpp
    storage/cache/storage_cache_database_object.h
    storage/cache/storage_cache_types.cpp
    storage/cache/storage_cache_types.h
    storage/storage_clear_legacy_posix.cpp
    storage/storage_clear_legacy_win.cpp

    storage/storage_pch.h
)

target_include_directories(lib_storage
PUBLIC
    ${src_loc}
)

target_link_libraries(lib_storage
PUBLIC
    desktop-app::lib_base
PRIVATE
    desktop-app::external_xxhash
)
