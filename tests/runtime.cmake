if(NOT COMMAND catch_discover_tests)
  find_package(grevir-test-support CONFIG REQUIRED)
endif()
add_executable(grevir_pulse_codec_runtime codec_test.cpp)
target_link_libraries(grevir_pulse_codec_runtime PRIVATE grevir::pulse_codec Catch2::Catch2WithMain)
set_target_properties(grevir_pulse_codec_runtime PROPERTIES CXX_EXTENSIONS OFF)
catch_discover_tests(grevir_pulse_codec_runtime TEST_PREFIX "pulse_codec."
  PROPERTIES LABELS "pulse_codec" TIMEOUT 10)
