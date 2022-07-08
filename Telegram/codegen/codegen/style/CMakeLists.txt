add_executable(codegen_style)
init_target(codegen_style "(codegen)")

get_filename_component(src_loc ../.. REALPATH)

nice_target_sources(codegen_style ${src_loc}
PRIVATE
    codegen/style/generator.cpp
    codegen/style/generator.h
    codegen/style/main.cpp
    codegen/style/module.cpp
    codegen/style/module.h
    codegen/style/options.cpp
    codegen/style/options.h
    codegen/style/parsed_file.cpp
    codegen/style/parsed_file.h
    codegen/style/processor.cpp
    codegen/style/processor.h
    codegen/style/structure_types.cpp
    codegen/style/structure_types.h
)

target_include_directories(codegen_style
PUBLIC
    ${src_loc}
)

target_link_libraries(codegen_style
PUBLIC
    desktop-app::codegen_common
    desktop-app::lib_base
    desktop-app::lib_crl
)
