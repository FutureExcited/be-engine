include(FetchContent)

# Every dependency is committed under vendor/. No network access at configure time.
# Upstream URLs and exact pinned refs: see vendor/VENDOR.md

# ── cpptrace ──────────────────────────────────────────────────────────────────
# cpptrace fetches libdwarf and zstd itself; redirect both to the vendored trees
# so that configuring never touches the network.
set(FETCHCONTENT_SOURCE_DIR_LIBDWARF "${CMAKE_CURRENT_SOURCE_DIR}/libdwarf" CACHE PATH "" FORCE)
set(FETCHCONTENT_SOURCE_DIR_ZSTD     "${CMAKE_CURRENT_SOURCE_DIR}/zstd"     CACHE PATH "" FORCE)

FetchContent_Declare(cpptrace
    SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/cpptrace"
    OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(cpptrace)

# ── libassert ─────────────────────────────────────────────────────────────────
set(LIBASSERT_USE_EXTERNAL_CPPTRACE ON CACHE BOOL "" FORCE)
FetchContent_Declare(libassert
    SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/libassert"
    OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(libassert)

# ── glfw ──────────────────────────────────────────────────────────────────────
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw EXCLUDE_FROM_ALL)

# ── assimp ────────────────────────────────────────────────────────────────────
set(ASSIMP_BUILD_TESTS                    OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS             OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES                  OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL                        OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS             OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX           OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER             ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER            ON  CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER             ON  CACHE BOOL "" FORCE)
add_subdirectory(Assimp EXCLUDE_FROM_ALL)

# ── lua ───────────────────────────────────────────────────────────────────────
file(GLOB LUA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/lua/src/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX "(lua|luac)\\.c$")
add_library(lua STATIC ${LUA_SOURCES})
target_include_directories(lua PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lua/src")
set_target_properties(lua PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ── header-only ───────────────────────────────────────────────────────────────
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/glm/include")

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann/include")

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/stb_image/include")

add_library(luabridge3 INTERFACE)
target_include_directories(luabridge3 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/luabridge3/Source")
target_link_libraries(luabridge3 INTERFACE lua)

# ── slang (prebuilt) ──────────────────────────────────────────────────────────
add_library(slang UNKNOWN IMPORTED GLOBAL)
target_include_directories(slang INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/slang/include")
if(WIN32)
    set_target_properties(slang PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/slang/lib/slang-compiler.lib")
elseif(APPLE)
    if(NOT DEFINED BE_SLANG_MACOS_ARCH)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
            set(BE_SLANG_MACOS_ARCH "aarch64")
        else()
            set(BE_SLANG_MACOS_ARCH "x86_64")
        endif()
    endif()

    set(BE_SLANG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/slang/macos-${BE_SLANG_MACOS_ARCH}" CACHE PATH "Shader Slang macOS binary root")
    set(BE_SLANG_LIB_DIR "${BE_SLANG_ROOT}/lib" CACHE INTERNAL "Shader Slang runtime library directory")

    if(NOT EXISTS "${BE_SLANG_ROOT}/include/slang.h" OR NOT EXISTS "${BE_SLANG_LIB_DIR}/libslang-compiler.dylib")
        message(FATAL_ERROR
            "Missing Shader Slang macOS binaries at ${BE_SLANG_ROOT}. "
            "Run ./tools/setup-macos-deps.sh from the repository root.")
    endif()

    target_include_directories(slang INTERFACE "${BE_SLANG_ROOT}/include")
    set_target_properties(slang PROPERTIES
        IMPORTED_LOCATION "${BE_SLANG_LIB_DIR}/libslang-compiler.dylib")
else()
    set_target_properties(slang PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/slang/linux-x64/lib/libslang-compiler.so")
endif()

# ── Vulkan (prebuilt) ─────────────────────────────────────────────────────────
add_library(Vulkan::Vulkan UNKNOWN IMPORTED GLOBAL)
target_include_directories(Vulkan::Vulkan INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vulkan-sdk/include")
if(WIN32)
    set_target_properties(Vulkan::Vulkan PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/vulkan-sdk/lib/vulkan-1.lib")
elseif(APPLE)
    if(NOT DEFINED BE_HOMEBREW_PREFIX)
        if(DEFINED ENV{HOMEBREW_PREFIX})
            set(BE_HOMEBREW_PREFIX "$ENV{HOMEBREW_PREFIX}")
        elseif(EXISTS "/opt/homebrew")
            set(BE_HOMEBREW_PREFIX "/opt/homebrew")
        elseif(EXISTS "/usr/local")
            set(BE_HOMEBREW_PREFIX "/usr/local")
        endif()
    endif()
    set(BE_HOMEBREW_PREFIX "${BE_HOMEBREW_PREFIX}" CACHE PATH "Homebrew prefix used for Vulkan loader and MoltenVK")
    set(BE_VULKAN_LIB_DIR "${BE_HOMEBREW_PREFIX}/lib" CACHE INTERNAL "Vulkan runtime library directory")
    set(BE_MOLTENVK_DYLIB "${BE_VULKAN_LIB_DIR}/libMoltenVK.dylib" CACHE FILEPATH "MoltenVK runtime dylib")

    if(NOT EXISTS "${BE_VULKAN_LIB_DIR}/libvulkan.1.dylib" OR NOT EXISTS "${BE_MOLTENVK_DYLIB}")
        message(FATAL_ERROR
            "Missing Homebrew Vulkan loader or MoltenVK under ${BE_HOMEBREW_PREFIX}. "
            "Install them with: brew install vulkan-loader molten-vk vulkan-headers")
    endif()

    set_target_properties(Vulkan::Vulkan PROPERTIES
        IMPORTED_LOCATION "${BE_VULKAN_LIB_DIR}/libvulkan.1.dylib")
else()
    set_target_properties(Vulkan::Vulkan PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/vulkan-sdk/lib/linux-x64/libvulkan.so.1")
endif()
