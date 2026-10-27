# Libraries and compiler settings of the Linux and macOS builds, included by the generated CMakeLists.txt in place of the
# MSVC settings of the Visual Studio project. Libraries with sources are fetched at configure time, at the versions whose
# headers are bundled in Libs. BASS and libVLC are bundled as binaries in Libs/<library>/macos and
# Libs/<library>/linux/<architecture>, with the libVLC plugins, so players need no system packages.

include(FetchContent)
enable_language(C)

if(APPLE)
    set(TEN_LIBRARY_PLATFORM macos)
    set(TEN_LIBRARY_SUFFIX .dylib)
    set(TEN_VLC_LIBRARY_NAME libvlc.dylib)
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(TEN_LIBRARY_PLATFORM linux/aarch64)
    else()
        set(TEN_LIBRARY_PLATFORM linux/x86_64)
    endif()
    set(TEN_LIBRARY_SUFFIX .so)
    set(TEN_VLC_LIBRARY_NAME libvlc.so.5)
endif()

# Definitions of the Visual Studio project that only make sense on Windows.
get_target_property(TEN_TARGET_DEFINITIONS TombEngine COMPILE_DEFINITIONS)
list(REMOVE_ITEM TEN_TARGET_DEFINITIONS WIN32 _WINDOWS _USRDLL _MBCS _CRT_SECURE_NO_WARNINGS TombEngine_EXPORTS)
set_property(TARGET TombEngine PROPERTY COMPILE_DEFINITIONS ${TEN_TARGET_DEFINITIONS})

# MSVC defines _DEBUG with the debug runtime, and the engine tests it for debug-only code.
target_compile_definitions(TombEngine PRIVATE "$<$<CONFIG:Debug>:_DEBUG>")

# GCC rejects members named like their type (ItemInfo::Pose), which MSVC and Clang accept.
target_compile_options(TombEngine PRIVATE
    "$<$<CONFIG:Debug>:-O0;-g>"
    "$<$<CONFIG:Release>:-O2;-ffast-math>"
    "$<$<CXX_COMPILER_ID:GNU>:-Wno-changes-meaning>"
)

# Exported symbols give function names to the stack trace of the crash handler.
target_link_options(TombEngine PRIVATE "$<$<PLATFORM_ID:Linux>:-rdynamic>")

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.18
    GIT_SHALLOW TRUE
)

FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.9.1
    GIT_SHALLOW TRUE
)

FetchContent_Declare(lz4
    GIT_REPOSITORY https://github.com/lz4/lz4.git
    GIT_TAG v1.10.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR build/cmake
)

FetchContent_Declare(lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.3.5
    GIT_SHALLOW TRUE
)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3 spdlog lz4 lua)

# Lua has no CMake project: the library is every source except the interpreter, the compiler and the test files.
file(GLOB TEN_LUA_SOURCES "${lua_SOURCE_DIR}/*.c")
list(FILTER TEN_LUA_SOURCES EXCLUDE REGEX "/(lua|luac|onelua|ltests)\\.c$")
add_library(TombEngineLua STATIC ${TEN_LUA_SOURCES})
target_compile_definitions(TombEngineLua PRIVATE "$<IF:$<BOOL:${APPLE}>,LUA_USE_MACOSX,LUA_USE_LINUX>")

set(TEN_BASS_DIR "${TEN_SOLUTION_DIR}/Libs/bass/${TEN_LIBRARY_PLATFORM}")
set(TEN_VLC_LIBRARY "${TEN_SOLUTION_DIR}/Libs/vlc/${TEN_LIBRARY_PLATFORM}/${TEN_VLC_LIBRARY_NAME}")
find_package(Threads REQUIRED)

target_link_libraries(TombEngine PRIVATE
    SDL3::SDL3
    spdlog::spdlog
    lz4_static
    TombEngineLua
    "${TEN_BASS_DIR}/libbass${TEN_LIBRARY_SUFFIX}"
    "${TEN_BASS_DIR}/libbassmix${TEN_LIBRARY_SUFFIX}"
    "${TEN_BASS_DIR}/libbass_fx${TEN_LIBRARY_SUFFIX}"
    "${TEN_VLC_LIBRARY}"
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

# The pre-build step copies BASS, libVLC and the plugins next to the executable, which loads them from its own folder;
# CMake adds the folder of the SDL it builds to the build tree search path. On Linux the search path is an RPATH rather
# than a RUNPATH, because only an RPATH of the executable also applies to the libraries of plugins loaded by libVLC.
if(APPLE)
    set_target_properties(TombEngine PROPERTIES BUILD_RPATH "@executable_path" INSTALL_RPATH "@executable_path")
else()
    set_target_properties(TombEngine PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
    target_link_options(TombEngine PRIVATE "-Wl,--disable-new-dtags")
endif()
