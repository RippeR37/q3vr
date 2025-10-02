# This ensures that all VCPKG dependencies are also installed by `cmake install`
# and are included in CPack installer.
# NOTE: it's not ideal as it is an experimental vcpkg feature, but its the
# cleanest way right now
set(X_VCPKG_APPLOCAL_DEPS_INSTALL TRUE CACHE BOOL "Install vcpkg dependencies")

# This allows configuring with pre-configured vcpkg via -DCMAKE_TOOLCHAIN_FILE=...
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  file(READ "${CMAKE_SOURCE_DIR}/vcpkg.json" VCPKG_JSON_CONTENT)
  string(REGEX MATCH "\"builtin-baseline\"[ \t]*:[ \t]*\"([a-f0-9]+)\"" _ "${VCPKG_JSON_CONTENT}")
  set(VCPKG_BUILTIN_BASELINE "${CMAKE_MATCH_1}")
  message("Fetching vcpkg at commit ${VCPKG_BUILTIN_BASELINE}")

  include(FetchContent)
  FetchContent_Declare(
      vcpkg
      GIT_REPOSITORY https://github.com/microsoft/vcpkg.git
      GIT_TAG        ${VCPKG_BUILTIN_BASELINE}
  )
  FetchContent_MakeAvailable(vcpkg)
  set(CMAKE_TOOLCHAIN_FILE "${vcpkg_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake")
endif()
