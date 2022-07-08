add_executable(codegen_emoji)
init_target(codegen_emoji "(codegen)")

get_filename_component(src_loc ../.. REALPATH)

nice_target_sources(codegen_emoji ${src_loc}
PRIVATE
    codegen/emoji/data.cpp
    codegen/emoji/data.h
    codegen/emoji/data_old.cpp
    codegen/emoji/data_old.h
    codegen/emoji/data_read.cpp
    codegen/emoji/data_read.h
    codegen/emoji/generator.cpp
    codegen/emoji/generator.h
    codegen/emoji/main.cpp
    codegen/emoji/options.cpp
    codegen/emoji/options.h
    codegen/emoji/replaces.cpp
    codegen/emoji/replaces.h
)

target_include_directories(codegen_emoji
PUBLIC
    ${src_loc}
)

target_link_libraries(codegen_emoji
PUBLIC
    desktop-app::lib_base
    desktop-app::codegen_common
    desktop-app::external_qt
)
