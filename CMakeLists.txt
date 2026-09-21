cmake_minimum_required(VERSION 3.20)
project(grevir_pulse_codec VERSION 0.1.0 LANGUAGES CXX)

if(NOT TARGET grevir::base)
  find_package(grevir-base CONFIG REQUIRED)
endif()

if(NOT TARGET grevir::time)
  find_package(grevir-time CONFIG REQUIRED)
endif()

add_library(grevir_pulse_codec INTERFACE)
add_library(grevir::pulse_codec ALIAS grevir_pulse_codec)
set_target_properties(grevir_pulse_codec PROPERTIES EXPORT_NAME pulse_codec)
target_compile_features(grevir_pulse_codec INTERFACE cxx_std_23)
target_link_libraries(grevir_pulse_codec INTERFACE grevir::base grevir::time)
target_include_directories(grevir_pulse_codec INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>)

include(GNUInstallDirs)
install(TARGETS grevir_pulse_codec EXPORT GrevirPulseCodecTargets)
install(DIRECTORY src/ DESTINATION include FILES_MATCHING
  PATTERN "*.hpp" PATTERN "*.h")
install(EXPORT GrevirPulseCodecTargets NAMESPACE grevir::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grevir-pulse-codec)
install(FILES cmake/grevir-pulse-codec-config.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grevir-pulse-codec)
install(FILES LICENSE.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/grevir-pulse-codec)

option(GREVIR_BUILD_COMPILE_CHECKS "Compile native headers and template users" OFF)
option(GREVIR_BUILD_HOST_TESTS "Run deterministic native behavior tests" OFF)
if(GREVIR_BUILD_HOST_TESTS)
  enable_testing()
endif()
if(GREVIR_BUILD_COMPILE_CHECKS OR GREVIR_BUILD_HOST_TESTS)
  add_subdirectory(tests)
endif()
