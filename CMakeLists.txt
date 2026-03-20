cmake_minimum_required(VERSION 3.21)
project(TombEngine VERSION 1.0.0 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Ensure consistent CRT linkage across all targets (including FetchContent deps).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "")

add_subdirectory(TombEngine)

# Make TombEngine the default startup project in Visual Studio (instead of ALL_BUILD).
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT TombEngine)

# ---------------------------------------------------------------------------
# CPack — Linux standalone distribution tarball
# ---------------------------------------------------------------------------
if(NOT WIN32)
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_NAME "TombEngine")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_SYSTEM_NAME "linux-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    set(CPACK_STRIP_FILES ON)
    include(CPack)
endif()
