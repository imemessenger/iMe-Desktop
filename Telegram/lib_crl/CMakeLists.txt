# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

add_library(lib_crl OBJECT)
add_library(desktop-app::lib_crl ALIAS lib_crl)
init_target(lib_crl)

get_filename_component(src_loc . REALPATH)

nice_target_sources(lib_crl ${src_loc}
PRIVATE
    crl/common/crl_common_config.h
    crl/common/crl_common_fp_exceptions.cpp
    crl/common/crl_common_guards.h
    crl/common/crl_common_list.cpp
    crl/common/crl_common_list.h
    crl/common/crl_common_object_async.h
    crl/common/crl_common_on_main.cpp
    crl/common/crl_common_on_main.h
    crl/common/crl_common_on_main_guarded.h
    crl/common/crl_common_queue.cpp
    crl/common/crl_common_queue.h
    crl/common/crl_common_sync.h
    crl/common/crl_common_utils.h
    crl/dispatch/crl_dispatch_async.cpp
    crl/dispatch/crl_dispatch_async.h
    crl/dispatch/crl_dispatch_on_main.h
    crl/dispatch/crl_dispatch_queue.cpp
    crl/dispatch/crl_dispatch_queue.h
    crl/dispatch/crl_dispatch_semaphore.cpp
    crl/dispatch/crl_dispatch_semaphore.h
    crl/mac/crl_mac_time.cpp
    crl/linux/crl_linux_time.cpp
    crl/qt/crl_qt_async.cpp
    crl/qt/crl_qt_async.h
    crl/qt/crl_qt_guards.h
    crl/qt/crl_qt_semaphore.cpp
    crl/qt/crl_qt_semaphore.h
    crl/winapi/crl_winapi_async.cpp
    crl/winapi/crl_winapi_async.h
    crl/winapi/crl_winapi_dll.h
    crl/winapi/crl_winapi_fp_exceptions.cpp
    crl/winapi/crl_winapi_list.cpp
    crl/winapi/crl_winapi_list.h
    crl/winapi/crl_winapi_semaphore.cpp
    crl/winapi/crl_winapi_semaphore.h
    crl/winapi/crl_winapi_time.cpp
    crl/winapi/crl_winapi_windows_h.h
    crl/crl.h
    crl/crl_async.h
    crl/crl_fp_exceptions.h
    crl/crl_object_on_queue.h
    crl/crl_object_on_thread.cpp
    crl/crl_object_on_thread.h
    crl/crl_on_main.h
    crl/crl_queue.h
    crl/crl_semaphore.h
    crl/crl_time.cpp
    crl/crl_time.h
)

target_include_directories(lib_crl
PUBLIC
    ${src_loc}
)

if (TARGET desktop-app::external_dispatch)
    target_link_libraries(lib_crl
    PUBLIC
        desktop-app::external_dispatch
    )
endif()

if (build_macstore OR TARGET desktop-app::external_dispatch)
    target_compile_definitions(lib_crl
    PUBLIC
        CRL_USE_COMMON_QUEUE
    )
endif()

target_link_libraries(lib_crl
PUBLIC
    desktop-app::external_qt
)
