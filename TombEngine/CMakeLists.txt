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
add_executable(TombEngine WIN32 ${ALL_SOURCES} ${ALL_HEADERS})
target_compile_features(TombEngine PRIVATE cxx_std_17)

# Precompiled headers
target_precompile_headers(TombEngine PRIVATE framework.h)
set_source_files_properties(../Libs/glad/glad.c
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

# Include self for #include "Game/...", "Renderer/...", etc.
target_include_directories(TombEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# Include directories — third-party libraries
# ---------------------------------------------------------------------------
set(LIBS_DIR ${CMAKE_SOURCE_DIR}/Libs)

target_include_directories(TombEngine PRIVATE
    ${LIBS_DIR}
    ${LIBS_DIR}/glad
    ${LIBS_DIR}/lua
    ${LIBS_DIR}/sol
    ${LIBS_DIR}/zlib
    ${LIBS_DIR}/lz4
    ${LIBS_DIR}/sdl3/include
    ${LIBS_DIR}/spdlog
    ${LIBS_DIR}/ois
    ${LIBS_DIR}/vlc
    ${LIBS_DIR}/bass
    ${LIBS_DIR}/srtparser
    ${LIBS_DIR}/glm
)

# ---------------------------------------------------------------------------
# Preprocessor definitions
# ---------------------------------------------------------------------------
target_compile_definitions(TombEngine PRIVATE
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    SPDLOG_COMPILED_LIB
    SOL_SAFE_USERTYPE
    SOL_SAFE_FUNCTION_CALLS
    ${RENDERER_DEFINES}
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
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(TombEngine PRIVATE
        -Wall
        -Wno-sign-compare
        -Wno-conversion
        -Wno-deprecated-declarations
    )
    target_compile_options(TombEngine PRIVATE
        $<$<CONFIG:Release>:-O2 -ffast-math>
    )
endif()

# ---------------------------------------------------------------------------
# Libraries
# ---------------------------------------------------------------------------
if(WIN32)
    # Determine arch-specific lib subdirectory
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCH_DIR x64)
    else()
        set(ARCH_DIR x86)
    endif()

    # Library search paths for prebuilt third-party libs
    target_link_directories(TombEngine PRIVATE
        ${LIBS_DIR}/spdlog/${ARCH_DIR}
        ${LIBS_DIR}/lua/${ARCH_DIR}
        ${LIBS_DIR}/lz4/${ARCH_DIR}
        ${LIBS_DIR}/sdl3/lib/x64
        ${LIBS_DIR}/vlc/${ARCH_DIR}
        ${LIBS_DIR}/bass/${ARCH_DIR}
        ${LIBS_DIR}/ois/${ARCH_DIR}
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
        $<IF:$<CONFIG:Debug>,sdl3,sdl3>
        $<IF:$<CONFIG:Debug>,spdlogd,spdlog>
        $<IF:$<CONFIG:Debug>,liblz4_static_d,liblz4_static>
        $<IF:$<CONFIG:Debug>,OIS_d,OIS>
    )

    # NuGet packages (XAudio2)
    set(NUGET_DIR ${CMAKE_SOURCE_DIR}/packages)

    set(XAUDIO2_DIR ${NUGET_DIR}/Microsoft.XAudio2.Redist.1.2.10)
    target_include_directories(TombEngine PRIVATE
        ${XAUDIO2_DIR}/build/native/include
    )
    target_link_libraries(TombEngine PRIVATE
        $<IF:$<CONFIG:Debug>,
            ${XAUDIO2_DIR}/build/native/debug/lib/${ARCH_DIR}/xaudio2_9redist.lib,
            ${XAUDIO2_DIR}/build/native/release/lib/${ARCH_DIR}/xaudio2_9redist.lib
        >
    )

else()
    # Linux / macOS — use system packages
    include(CMakeFindDependencyMacro)
    find_package(PkgConfig REQUIRED)

    find_package(SDL3 REQUIRED)
    find_package(OpenGL REQUIRED)

    pkg_check_modules(SPDLOG REQUIRED spdlog)
    pkg_check_modules(LZ4    REQUIRED liblz4)
    pkg_check_modules(LUA    REQUIRED lua5.3)

    target_link_libraries(TombEngine PRIVATE
        SDL3::SDL3
        OpenGL::GL
        ${SPDLOG_LIBRARIES}
        ${LZ4_LIBRARIES}
        ${LUA_LIBRARIES}
    )
    target_include_directories(TombEngine PRIVATE
        ${SPDLOG_INCLUDE_DIRS}
        ${LZ4_INCLUDE_DIRS}
        ${LUA_INCLUDE_DIRS}
    )
endif()

# ---------------------------------------------------------------------------
# Post-build: copy shaders, scripts, and DLLs to output directory
# ---------------------------------------------------------------------------
if(WIN32)
    # Determine arch-specific lib subdirectory (reuse ARCH_DIR from above)
    set(DLL_DIRS
        ${LIBS_DIR}/bass/${ARCH_DIR}
        ${LIBS_DIR}/lua/${ARCH_DIR}
        ${LIBS_DIR}/ois/${ARCH_DIR}
        ${LIBS_DIR}/vlc/${ARCH_DIR}
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
# IDE source grouping (Visual Studio, Xcode, CLion)
# ---------------------------------------------------------------------------
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ALL_SOURCES} ${ALL_HEADERS})
