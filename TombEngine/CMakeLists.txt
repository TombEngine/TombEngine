# TombEngine — main target CMakeLists.txt

include(Sources.cmake)

# ---------------------------------------------------------------------------
# Platform-specific renderer selection
# ---------------------------------------------------------------------------
if(WIN32)
    set(RENDERER_SOURCES
        ${TEN_RENDERER_SOURCES}
        ${TEN_RENDERER_DX11_SOURCES}
        ${TEN_RENDERER_OPENGL_SOURCES}
    )
    set(RENDERER_HEADERS
        ${TEN_RENDERER_HEADERS}
        ${TEN_RENDERER_DX11_HEADERS}
        ${TEN_RENDERER_OPENGL_HEADERS}
    )
    set(RENDERER_DEFINES HAS_DX11 HAS_OPENGL)
else()
    set(RENDERER_SOURCES
        ${TEN_RENDERER_SOURCES}
        ${TEN_RENDERER_OPENGL_SOURCES}
    )
    set(RENDERER_HEADERS
        ${TEN_RENDERER_HEADERS}
        ${TEN_RENDERER_OPENGL_HEADERS}
    )
    set(RENDERER_DEFINES HAS_OPENGL)
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

# Precompiled headers
target_precompile_headers(TombEngine PRIVATE framework.h)
set_source_files_properties(../Libs/glad/glad.c
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

# GCC requires optimization for SSE intrinsics used by stb_image, even in Debug.
if(NOT MSVC)
    set_source_files_properties(Renderer/stb_impl.cpp
        PROPERTIES COMPILE_OPTIONS "-O2"
    )
endif()

# Include self for #include "Game/...", "Renderer/...", etc.
target_include_directories(TombEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# Include directories — third-party libraries
# ---------------------------------------------------------------------------
set(LIBS_DIR ${CMAKE_SOURCE_DIR}/Libs)

# Header-only / always-vendored libraries (all platforms).
# On non-Windows, use -idirafter for ${LIBS_DIR} so that FetchContent spdlog
# headers (added via spdlog::spdlog target) take priority over the vendored
# spdlog in Libs/spdlog/.
if(WIN32)
    target_include_directories(TombEngine PRIVATE ${LIBS_DIR})
else()
    target_compile_options(TombEngine PRIVATE -idirafter ${LIBS_DIR})
endif()

target_include_directories(TombEngine PRIVATE
    ${LIBS_DIR}/glad
    ${LIBS_DIR}/sol
    ${LIBS_DIR}/srtparser
    ${LIBS_DIR}/glm
    ${LIBS_DIR}/bass
)

# On Windows, all third-party headers come from Libs/.
if(WIN32)
    target_include_directories(TombEngine PRIVATE
        ${LIBS_DIR}/lua
        ${LIBS_DIR}/lz4
        ${LIBS_DIR}/sdl3/include
        ${LIBS_DIR}/spdlog
        ${LIBS_DIR}/vlc
    )
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
        ${LIBS_DIR}/spdlog/x64
        ${LIBS_DIR}/lua/x64
        ${LIBS_DIR}/lz4/x64
        ${LIBS_DIR}/sdl3/lib/x64
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
        opengl32
    )

    # Third-party prebuilt libraries
    target_link_libraries(TombEngine PRIVATE
        lua53
        bass
        bassmix
        bass_fx
        libvlc
        libvlccore
        sdl3
        $<IF:$<CONFIG:Debug>,spdlogd,spdlog>
        liblz4_static
    )

else()
    # Linux / macOS — use system packages where available,
    # prebuilt libraries from Libs/ for BASS and VLC.
    include(CMakeFindDependencyMacro)
    include(FetchContent)

    find_package(SDL3 QUIET)
    if(NOT SDL3_FOUND)
        FetchContent_Declare(SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG        release-3.2.8
        )
        FetchContent_MakeAvailable(SDL3)
    endif()

    # Build spdlog from source so that headers and library ABI always match.
    # The vendored Libs/spdlog/ headers use fmt v8 which is incompatible with
    # system spdlog/fmt packages; building from source avoids this mismatch.
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.10.0
    )
    set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog)

    find_package(OpenGL REQUIRED)

    # Build LZ4 from source via FetchContent (no system liblz4-dev needed).
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

    # Build Lua 5.3 from source via FetchContent (no system liblua5.3-dev needed).
    FetchContent_Declare(lua
        URL      https://www.lua.org/ftp/lua-5.3.6.tar.gz
        URL_HASH SHA256=fc5fd69bb8736323f026672b1b7235da613d7177e72558893a0bdcd320466d60
    )
    FetchContent_MakeAvailable(lua)
    file(GLOB LUA_CORE_SOURCES ${lua_SOURCE_DIR}/src/*.c)
    list(REMOVE_ITEM LUA_CORE_SOURCES
        ${lua_SOURCE_DIR}/src/lua.c
        ${lua_SOURCE_DIR}/src/luac.c
    )
    add_library(lua53_static STATIC ${LUA_CORE_SOURCES})
    target_include_directories(lua53_static PUBLIC ${lua_SOURCE_DIR}/src)
    target_compile_definitions(lua53_static PRIVATE LUA_USE_POSIX LUA_USE_DLOPEN)
    target_link_libraries(lua53_static PRIVATE m dl)

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
        target_link_directories(TombEngine PRIVATE
            ${LIBS_DIR}/bass/${BASS_LIB_DIR}
            ${LIBS_DIR}/vlc/${VLC_LIB_DIR}
        )
        target_include_directories(TombEngine PRIVATE ${LIBS_DIR}/vlc)
    endif()

    target_link_libraries(TombEngine PRIVATE
        SDL3::SDL3
        OpenGL::GL
        spdlog::spdlog
        lz4_static
        lua53_static
        bass
        bassmix
        bass_fx
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
    set(DLL_DIRS
        ${LIBS_DIR}/bass/x64
        ${LIBS_DIR}/lua/x64
        ${LIBS_DIR}/vlc/x64
        ${LIBS_DIR}/sdl3/lib/x64
    )

    foreach(DLL_DIR ${DLL_DIRS})
        add_custom_command(TARGET TombEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "${DLL_DIR}" "$<TARGET_FILE_DIR:TombEngine>"
            COMMAND_EXPAND_LISTS
        )
    endforeach()

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

# Copy shaders
add_custom_command(TARGET TombEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
        "$<TARGET_FILE_DIR:TombEngine>/../Shaders"
)

# Copy engine scripts
add_custom_command(TARGET TombEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Scripts/Engine"
        "$<TARGET_FILE_DIR:TombEngine>/../Scripts/Engine"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Scripts/SystemStrings.lua"
        "$<TARGET_FILE_DIR:TombEngine>/../Scripts/SystemStrings.lua"
)

# ---------------------------------------------------------------------------
# Install rules (Linux standalone distribution)
# ---------------------------------------------------------------------------
if(NOT WIN32)
    # Binary goes into Engine/ subdirectory.
    install(TARGETS TombEngine RUNTIME DESTINATION Engine)

    # BASS shared libraries.
    file(GLOB _INSTALL_BASS_LIBS
        "${LIBS_DIR}/bass/${BASS_LIB_DIR}/*.so"
        "${LIBS_DIR}/bass/${BASS_LIB_DIR}/*.dylib"
    )
    install(FILES ${_INSTALL_BASS_LIBS} DESTINATION Engine)

    if(NOT APPLE)
        # VLC shared libraries.
        file(GLOB _INSTALL_VLC_LIBS
            "${LIBS_DIR}/vlc/${VLC_LIB_DIR}/*.so"
            "${LIBS_DIR}/vlc/${VLC_LIB_DIR}/*.so.*"
        )
        install(FILES ${_INSTALL_VLC_LIBS} DESTINATION Engine)

        # VLC plugins.
        if(EXISTS "${LIBS_DIR}/vlc/${VLC_LIB_DIR}/plugins")
            install(DIRECTORY "${LIBS_DIR}/vlc/${VLC_LIB_DIR}/plugins"
                    DESTINATION Engine)
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
# IDE source grouping (Visual Studio, Xcode, CLion)
# ---------------------------------------------------------------------------
# Filter out files outside the TombEngine/ tree (e.g. Libs/glad/glad.c)
# before calling source_group, which requires all files to share the ROOT prefix.
set(_GROUPABLE_FILES ${ALL_SOURCES} ${ALL_HEADERS})
list(FILTER _GROUPABLE_FILES INCLUDE REGEX "^${CMAKE_CURRENT_SOURCE_DIR}")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${_GROUPABLE_FILES})
