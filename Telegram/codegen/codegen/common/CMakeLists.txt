add_library(codegen_common OBJECT)
add_library(desktop-app::codegen_common ALIAS codegen_common)
init_target(codegen_common "(codegen)")

get_filename_component(src_loc ../.. REALPATH)

nice_target_sources(codegen_common ${src_loc}
PRIVATE
    codegen/common/basic_tokenized_file.cpp
    codegen/common/basic_tokenized_file.h
    codegen/common/checked_utf8_string.cpp
    codegen/common/checked_utf8_string.h
    codegen/common/clean_file.cpp
    codegen/common/clean_file.h
    codegen/common/clean_file_reader.h
    codegen/common/const_utf8_string.h
    codegen/common/cpp_file.cpp
    codegen/common/cpp_file.h
    codegen/common/logging.cpp
    codegen/common/logging.h
)

target_include_directories(codegen_common
PUBLIC
    ${src_loc}
)

target_link_libraries(codegen_common
PUBLIC
    desktop-app::external_qt
)
