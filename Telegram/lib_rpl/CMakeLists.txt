# This file is part of Desktop App Toolkit,
# a set of libraries for developing nice desktop applications.
#
# For license and copyright information please follow this link:
# https://github.com/desktop-app/legal/blob/master/LEGAL

add_library(lib_rpl INTERFACE)
add_library(desktop-app::lib_rpl ALIAS lib_rpl)

get_filename_component(src_loc "." REALPATH)

nice_target_sources(lib_rpl ${src_loc}
INTERFACE
    rpl/details/callable.h
    rpl/details/superset_type.h
    rpl/details/type_list.h
    rpl/after_next.h
    rpl/before_next.h
    rpl/combine.h
    rpl/combine_previous.h
    rpl/complete.h
    rpl/conditional.h
    rpl/consumer.h
    rpl/deferred.h
    rpl/distinct_until_changed.h
    rpl/event_stream.h
    rpl/fail.h
    rpl/filter.h
    rpl/flatten_latest.h
    rpl/lifetime.h
    rpl/map.h
    rpl/mappers.h
    rpl/merge.h
    rpl/never.h
    rpl/producer.h
    rpl/range.h
    rpl/rpl.h
    rpl/skip.h
    rpl/take.h
    rpl/then.h
    rpl/type_erased.h
    rpl/variable.h
)

target_include_directories(lib_rpl
INTERFACE
    ${src_loc}
)

target_link_libraries(lib_rpl
INTERFACE
    desktop-app::external_gsl
)
