cmake_minimum_required(VERSION 3.8)

function (tl_add_library name)
  cmake_parse_arguments(ARG "" "" "SOURCES")
  add_library(${name} INTERFACE)
  target_sources(${name} INTERFACE 
                 $<BUILD_INTERFACE:${ARG_SOURCES}>)
  target_include_directories(${name} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
 )

  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/tl-${name}-config-version.cmake"
    COMPATIBILITY SameMajorVersion
)

  include(GNUInstallDirs)
  install(TARGETS ${name}
    EXPORT tl-targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/tl-${name}-config.cmake.in"
    "${PROJECT_BINARY_DIR}/tl-${name}-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/tl-${name}
  )

  install(EXPORT tl-targets
    FILE
      tl-${name}-targets.cmake
    NAMESPACE
      tl::
    DESTINATION
      ${CMAKE_INSTALL_DATADIR}/cmake/tl-${name}
  )

  install(FILES "${PROJECT_BINARY_DIR}/tl-${name}-config-version.cmake"
                "${PROJECT_BINARY_DIR}/tl-${name}-config.cmake"
          DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/tl-${name})
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endfunction()
