cmake_minimum_required(VERSION 3.21)
project(TombEngine VERSION 1.0.0 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_subdirectory(TombEngine)

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
