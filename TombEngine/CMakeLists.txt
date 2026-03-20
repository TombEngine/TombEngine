# TombEngine — main target CMakeLists.txt

include(Sources.cmake)

# ---------------------------------------------------------------------------
# Platform subdirectory for output layout (Bin/Windows, Bin/Linux, etc.)
# ---------------------------------------------------------------------------
if(WIN32)
    set(TEN_PLATFORM_SUBDIR "Windows")
elseif(APPLE)
    set(TEN_PLATFORM_SUBDIR "macOS")
else()
    set(TEN_PLATFORM_SUBDIR "Linux")
endif()

# ---------------------------------------------------------------------------
# Platform-specific renderer selection
# ---------------------------------------------------------------------------
if(WIN32)
    set(RENDERER_SOURCES
        ${TEN_RENDERER_SOURCES}
        ${TEN_RENDERER_DX11_SOURCES}
        ${TEN_RENDERER_SDLGPU_SOURCES}
    )
    set(RENDERER_HEADERS
        ${TEN_RENDERER_HEADERS}
        ${TEN_RENDERER_DX11_HEADERS}
        ${TEN_RENDERER_SDLGPU_HEADERS}
    )
    set(RENDERER_DEFINES HAS_DX11 HAS_SDLGPU)
else()
    set(RENDERER_SOURCES
        ${TEN_RENDERER_SOURCES}
        ${TEN_RENDERER_SDLGPU_SOURCES}
    )
    set(RENDERER_HEADERS
        ${TEN_RENDERER_HEADERS}
        ${TEN_RENDERER_SDLGPU_HEADERS}
    )
    set(RENDERER_DEFINES HAS_SDLGPU)
endif()

# ---------------------------------------------------------------------------
# Collect all sources
# ---------------------------------------------------------------------------
set(ALL_SOURCES
    ${TEN_PCH_SOURCE}
    ${TEN_GAME_SOURCES}
    ${TEN_MATH_SOURCES}
    ${TEN_OBJECTS_SOURCES}
    ${TEN_PHYSICS_SOURCES}
    ${RENDERER_SOURCES}
    ${TEN_SCRIPTING_SOURCES}
    ${TEN_SOUND_SOURCES}
    ${TEN_SPECIFIC_SOURCES}
    ${TEN_PLATFORM_SOURCES}
    ${TEN_THIRDPARTY_SOURCES}
)

set(ALL_HEADERS
    ${TEN_ROOT_HEADERS}
    ${TEN_GAME_HEADERS}
    ${TEN_MATH_HEADERS}
    ${TEN_OBJECTS_HEADERS}
    ${TEN_PHYSICS_HEADERS}
    ${RENDERER_HEADERS}
    ${TEN_SCRIPTING_HEADERS}
    ${TEN_SOUND_HEADERS}
    ${TEN_SPECIFIC_HEADERS}
    ${TEN_PLATFORM_HEADERS}
)

# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------
if(WIN32)
    add_executable(TombEngine WIN32 ${ALL_SOURCES} ${ALL_HEADERS})
else()
    add_executable(TombEngine ${ALL_SOURCES} ${ALL_HEADERS})
endif()
target_compile_features(TombEngine PRIVATE cxx_std_17)

# Output directory: place the final binary under Build/<Config>/Bin/<Platform>/
# using an absolute path so the exe always lands in the same location regardless
# of which preset (and binaryDir) is active.
foreach(_CONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${_CONFIG} _CONFIG_UPPER)
    set_target_properties(TombEngine PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${_CONFIG_UPPER}
            "${CMAKE_SOURCE_DIR}/Build/${_CONFIG}/Bin/${TEN_PLATFORM_SUBDIR}"
    )
endforeach()
# Single-config fallback (e.g. plain Ninja or Makefiles).
if(CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set_target_properties(TombEngine PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_SOURCE_DIR}/Build/${CMAKE_BUILD_TYPE}/Bin/${TEN_PLATFORM_SUBDIR}"
    )
endif()

# Precompiled headers
target_precompile_headers(TombEngine PRIVATE framework.h)

# GCC requires optimization for SSE intrinsics used by stb_image, even in Debug.
if(NOT MSVC)
    set_source_files_properties(Renderer/stb_impl.cpp
        PROPERTIES COMPILE_OPTIONS "-O2"
    )
endif()

# Include self for #include "Game/...", "Renderer/...", etc.
target_include_directories(TombEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# FetchContent dependencies (all platforms)
# ---------------------------------------------------------------------------
set(LIBS_DIR ${CMAKE_SOURCE_DIR}/Libs)
include(FetchContent)

# spdlog — built from source on all platforms for CRT consistency.
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.10.0
)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# SDL3 — built from source on all platforms.
find_package(SDL3 QUIET)
if(NOT SDL3_FOUND)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.2.8
    )
    FetchContent_MakeAvailable(SDL3)
endif()

# SDL_shadercross — pinned to a specific commit for reproducibility.
FetchContent_Declare(SDL_shadercross
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_shadercross.git
    GIT_TAG        3a314481ede01f5d966f0e834019942e34c02b82  # Pinned 2026-03-19
)
set(SDLSHADERCROSS_SHARED OFF CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_INSTALL OFF CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_CLI OFF CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_VENDORED ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL_shadercross)

# LZ4 — built from source on all platforms.
FetchContent_Declare(lz4
    GIT_REPOSITORY https://github.com/lz4/lz4.git
    GIT_TAG        v1.10.0
    SOURCE_SUBDIR  build/cmake
)
set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LZ4_BUILD_LEGACY_LZ4C OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(lz4)

# stb — header-only image loading/writing (stb_image.h, stb_image_write.h).
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        28d546d5eb77d4585506a20480f4de2e706dff4c
)
FetchContent_MakeAvailable(stb)
# stb is header-only with no CMakeLists.txt; just expose the source dir.
target_include_directories(TombEngine PRIVATE ${stb_SOURCE_DIR})

# Lua 5.3 — built from source on all platforms (static library).
FetchContent_Declare(lua
    URL      https://www.lua.org/ftp/lua-5.3.6.tar.gz
    URL_HASH SHA256=fc5fd69bb8736323f026672b1b7235da613d7177e72558893a0bdcd320466d60
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lua)
file(GLOB LUA_CORE_SOURCES ${lua_SOURCE_DIR}/src/*.c)
list(REMOVE_ITEM LUA_CORE_SOURCES
    ${lua_SOURCE_DIR}/src/lua.c
    ${lua_SOURCE_DIR}/src/luac.c
)
add_library(lua53_static STATIC ${LUA_CORE_SOURCES})
target_include_directories(lua53_static PUBLIC ${lua_SOURCE_DIR}/src)
if(NOT WIN32)
    target_compile_definitions(lua53_static PRIVATE LUA_USE_POSIX LUA_USE_DLOPEN)
    target_link_libraries(lua53_static PRIVATE m dl)
endif()

# ---------------------------------------------------------------------------
# Include directories — third-party libraries
# ---------------------------------------------------------------------------
target_include_directories(TombEngine PRIVATE
    ${LIBS_DIR}
    ${LIBS_DIR}/sol
    ${LIBS_DIR}/srtparser
    ${LIBS_DIR}/glm
    ${LIBS_DIR}/bass
)

# VLC headers.
if(WIN32)
    target_include_directories(TombEngine PRIVATE ${LIBS_DIR}/vlc)
endif()

# ---------------------------------------------------------------------------
# Preprocessor definitions
# ---------------------------------------------------------------------------
target_compile_definitions(TombEngine PRIVATE
    SPDLOG_COMPILED_LIB
    SOL_SAFE_USERTYPE
    SOL_SAFE_FUNCTION_CALLS
    ${RENDERER_DEFINES}
    $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
)

if(WIN32)
    target_compile_definitions(TombEngine PRIVATE
        WIN32
        TombEngine_EXPORTS
        _WINDOWS
        _USRDLL
    )
endif()

# Debug-only defines
target_compile_definitions(TombEngine PRIVATE
    $<$<CONFIG:Debug>:CREATURE_AI_PRIORITY_OPTIMIZATION>
)

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
if(MSVC)
    target_compile_options(TombEngine PRIVATE
        /Zc:__cplusplus
        /experimental:external
        /external:anglebrackets
        /bigobj
        /MP
        /FS
        /WX
        /wd4018
        /wd4244
        /wd4996
    )

    # Release optimisations
    target_compile_options(TombEngine PRIVATE
        $<$<CONFIG:Release>:/O2 /Oi /Ot /GL /Gy /fp:fast>
    )
    target_link_options(TombEngine PRIVATE
        $<$<CONFIG:Release>:/LTCG>
        /ENTRY:mainCRTStartup
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(TombEngine PRIVATE
        -Wall
        -Wno-sign-compare
        -Wno-conversion
        -Wno-deprecated-declarations
        -Wno-changes-meaning
        -Wno-unused-function
        -fpermissive
        -Wno-parentheses
        -Wno-unused-variable
        -Wno-unused-but-set-variable
        -Wno-strict-aliasing
        -Wno-switch
        -Wno-class-memaccess
        -Wno-format-security
        -Wno-int-in-bool-context
        -Wno-maybe-uninitialized
        -Wno-misleading-indentation
        -Wno-pointer-arith
        -Wno-narrowing
    )
    target_compile_options(TombEngine PRIVATE
        $<$<CONFIG:Release>:-O2 -ffast-math>
    )
endif()

# ---------------------------------------------------------------------------
# Libraries
# ---------------------------------------------------------------------------

if(WIN32)
    # Library search paths for prebuilt third-party libs (x64 only)
    target_link_directories(TombEngine PRIVATE
        ${LIBS_DIR}/vlc/x64
        ${LIBS_DIR}/bass/x64
    )

    # System libraries
    target_link_libraries(TombEngine PRIVATE
        comctl32
        D3DCompiler
        dbghelp
        dxgi
        dxguid
        d3d11
        version
    )

    # Third-party libraries
    target_link_libraries(TombEngine PRIVATE
        SDL3::SDL3
        spdlog::spdlog
        lz4_static
        lua53_static
        SDL3_shadercross::SDL3_shadercross-static
        bass
        bassmix
        bass_fx
        libvlc
        libvlccore
    )

else()
    # Linux / macOS — use system packages where available,
    # prebuilt libraries from Libs/ for BASS and VLC.

    # Determine platform-specific lib subdirectory for prebuilt libs.
    if(APPLE)
        set(BASS_LIB_DIR macos)  # Universal binaries (x86_64 + arm64).
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(VLC_LIB_DIR macos/arm64)
        else()
            set(VLC_LIB_DIR macos/x86_64)
        endif()
        set(VLC_LIBRARIES vlc vlccore)
        target_link_directories(TombEngine PRIVATE
            ${LIBS_DIR}/bass/${BASS_LIB_DIR}
            ${LIBS_DIR}/vlc/${VLC_LIB_DIR}
        )
    else()
        # Linux: BASS and VLC from prebuilt libs in Libs/.
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
            set(BASS_LIB_DIR linux/aarch64)
            set(VLC_LIB_DIR linux/aarch64)
        else()
            set(BASS_LIB_DIR linux/x86_64)
            set(VLC_LIB_DIR linux/x86_64)
        endif()

        # Auto-download VLC libraries if not present.
        if(NOT EXISTS "${LIBS_DIR}/vlc/${VLC_LIB_DIR}/libvlc.so")
            message(STATUS "VLC libraries not found, downloading...")
            execute_process(
                COMMAND bash "${CMAKE_SOURCE_DIR}/Tools/download-vlc-linux.sh" "${CMAKE_SYSTEM_PROCESSOR}"
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                RESULT_VARIABLE VLC_DL_RESULT
            )
            if(NOT VLC_DL_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to download VLC libraries. Run manually: bash Tools/download-vlc-linux.sh ${CMAKE_SYSTEM_PROCESSOR}")
            endif()
        endif()

        target_link_directories(TombEngine PRIVATE
            ${LIBS_DIR}/bass/${BASS_LIB_DIR}
            ${LIBS_DIR}/vlc/${VLC_LIB_DIR}
        )
        target_include_directories(TombEngine PRIVATE ${LIBS_DIR}/vlc)
    endif()

    find_package(TBB REQUIRED)

    target_link_libraries(TombEngine PRIVATE
        SDL3::SDL3
        spdlog::spdlog
        lz4_static
        lua53_static
        TBB::tbb
        bass
        bassmix
        bass_fx
        SDL3_shadercross::SDL3_shadercross-static
        vlc
        vlccore
    )

    # RPATH so the executable finds BASS .so/.dylib next to itself.
    set_target_properties(TombEngine PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_RPATH   "$ORIGIN"
    )
endif()

# ---------------------------------------------------------------------------
# Post-build: copy shaders, scripts, and DLLs to output directory
# ---------------------------------------------------------------------------
if(WIN32)
    # Copy prebuilt DLLs (BASS, VLC).
    set(DLL_DIRS
        ${LIBS_DIR}/bass/x64
        ${LIBS_DIR}/vlc/x64
    )

    foreach(DLL_DIR ${DLL_DIRS})
        add_custom_command(TARGET TombEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "${DLL_DIR}" "$<TARGET_FILE_DIR:TombEngine>"
            COMMAND_EXPAND_LISTS
        )
    endforeach()

    # Copy SDL3.dll from FetchContent build tree.
    add_custom_command(TARGET TombEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:TombEngine>"
    )

    # Copy dxcompiler.dll from SDL_shadercross build tree (needed at runtime
    # for HLSL -> SPIRV compilation in the SDL_GPU backend).
    # When DXC is built from source (SDLSHADERCROSS_VENDORED), the DXIL
    # validator is statically linked into dxcompiler.dll — no separate dxil.dll.
    if(TARGET dxcompiler)
        add_custom_command(TARGET TombEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:dxcompiler>" "$<TARGET_FILE_DIR:TombEngine>"
        )
    endif()

    # Clean non-DLL files that got copied from the lib directories
    add_custom_command(TARGET TombEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f
            "$<TARGET_FILE_DIR:TombEngine>/*.lib"
            "$<TARGET_FILE_DIR:TombEngine>/*.pdb"
        VERBATIM
    )
else()
    # Copy BASS shared libraries next to the executable.
    file(GLOB BASS_SHARED_LIBS "${LIBS_DIR}/bass/${BASS_LIB_DIR}/*.so" "${LIBS_DIR}/bass/${BASS_LIB_DIR}/*.dylib")
    foreach(BASS_LIB ${BASS_SHARED_LIBS})
        add_custom_command(TARGET TombEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${BASS_LIB}" "$<TARGET_FILE_DIR:TombEngine>"
        )
    endforeach()

    # Copy VLC shared libraries next to the executable (Linux only).
    if(NOT APPLE)
        file(GLOB VLC_SHARED_LIBS "${LIBS_DIR}/vlc/${VLC_LIB_DIR}/*.so" "${LIBS_DIR}/vlc/${VLC_LIB_DIR}/*.so.*")
        foreach(VLC_LIB ${VLC_SHARED_LIBS})
            add_custom_command(TARGET TombEngine POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${VLC_LIB}" "$<TARGET_FILE_DIR:TombEngine>"
            )
        endforeach()

        # Copy VLC plugins directory if present.
        if(EXISTS "${LIBS_DIR}/vlc/${VLC_LIB_DIR}/plugins")
            add_custom_command(TARGET TombEngine POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${LIBS_DIR}/vlc/${VLC_LIB_DIR}/plugins"
                    "$<TARGET_FILE_DIR:TombEngine>/plugins"
            )
        endif()
    endif()
endif()

# Copy shaders (two levels up from Bin/<Platform>/)
add_custom_command(TARGET TombEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
        "$<TARGET_FILE_DIR:TombEngine>/../../Shaders"
)

# Copy engine scripts (two levels up from Bin/<Platform>/)
add_custom_command(TARGET TombEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Scripts/Engine"
        "$<TARGET_FILE_DIR:TombEngine>/../../Scripts/Engine"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Scripts/SystemStrings.lua"
        "$<TARGET_FILE_DIR:TombEngine>/../../Scripts/SystemStrings.lua"
)

# ---------------------------------------------------------------------------
# Install rules (Linux standalone distribution)
# ---------------------------------------------------------------------------
if(NOT WIN32)
    set(_INSTALL_ENGINE_DIR "Bin/${TEN_PLATFORM_SUBDIR}")

    # Binary goes into Bin/<Platform>/ subdirectory.
    install(TARGETS TombEngine RUNTIME DESTINATION ${_INSTALL_ENGINE_DIR})

    # BASS shared libraries.
    file(GLOB _INSTALL_BASS_LIBS
        "${LIBS_DIR}/bass/${BASS_LIB_DIR}/*.so"
        "${LIBS_DIR}/bass/${BASS_LIB_DIR}/*.dylib"
    )
    install(FILES ${_INSTALL_BASS_LIBS} DESTINATION ${_INSTALL_ENGINE_DIR})

    if(NOT APPLE)
        # VLC shared libraries.
        file(GLOB _INSTALL_VLC_LIBS
            "${LIBS_DIR}/vlc/${VLC_LIB_DIR}/*.so"
            "${LIBS_DIR}/vlc/${VLC_LIB_DIR}/*.so.*"
        )
        install(FILES ${_INSTALL_VLC_LIBS} DESTINATION ${_INSTALL_ENGINE_DIR})

        # VLC plugins.
        if(EXISTS "${LIBS_DIR}/vlc/${VLC_LIB_DIR}/plugins")
            install(DIRECTORY "${LIBS_DIR}/vlc/${VLC_LIB_DIR}/plugins"
                    DESTINATION ${_INSTALL_ENGINE_DIR})
        endif()
    endif()

    # Shaders.
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
            DESTINATION .)

    # Engine scripts.
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/Scripts/Engine"
            DESTINATION Scripts)
    install(FILES "${CMAKE_SOURCE_DIR}/Scripts/SystemStrings.lua"
            DESTINATION Scripts)

    # Launcher script.
    install(PROGRAMS "${CMAKE_SOURCE_DIR}/packaging/linux/TombEngine.sh"
            DESTINATION .)
endif()

# ---------------------------------------------------------------------------
# Shader files (not compiled by CMake, but visible in IDE)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SHADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.hlsli"
    "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.glsl"
)
target_sources(TombEngine PRIVATE ${SHADER_FILES})

# ---------------------------------------------------------------------------
# IDE source grouping (Visual Studio, Xcode, CLion)
# ---------------------------------------------------------------------------
# Sources.cmake uses relative paths; make them absolute so source_group(TREE) works.
set(_GROUPABLE_FILES)
foreach(_f ${ALL_SOURCES} ${ALL_HEADERS} ${SHADER_FILES})
    if(IS_ABSOLUTE "${_f}")
        list(APPEND _GROUPABLE_FILES "${_f}")
    else()
        list(APPEND _GROUPABLE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
    endif()
endforeach()
# Keep only files inside the TombEngine/ tree.
list(FILTER _GROUPABLE_FILES INCLUDE REGEX "^${CMAKE_CURRENT_SOURCE_DIR}")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${_GROUPABLE_FILES})
