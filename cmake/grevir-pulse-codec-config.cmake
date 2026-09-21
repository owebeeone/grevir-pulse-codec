include(CMakeFindDependencyMacro)
find_dependency(grevir-base CONFIG)
find_dependency(grevir-time CONFIG)
include("${CMAKE_CURRENT_LIST_DIR}/GrevirPulseCodecTargets.cmake")
