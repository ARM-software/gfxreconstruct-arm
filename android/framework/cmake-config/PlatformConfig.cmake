set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED ON)

# because the NDK sets CMAKE_FIND_ROOT_PATH_MODE_*  to ONLY, it ignores
# us setting CMAKE_PREFIX_PATH.  So just set the package DIR directly.
set(nlohmann_json_DIR "${GFXRECON_SOURCE_DIR}/external/nlohmann-json/share/cmake/nlohmann_json")
find_package(nlohmann_json REQUIRED CONFIG PATHS "${nlohmann_json_DIR}" NO_DEFAULT_PATH)

set(CMAKE_MODULE_PATH "${GFXRECON_SOURCE_DIR}/external/cmake-modules")
list(APPEND CMAKE_MODULE_PATH "${GFXRECON_SOURCE_DIR}/cmake")

# Version info
set(GFXRECONSTRUCT_PROJECT_VERSION_MAJOR 1)
set(GFXRECONSTRUCT_PROJECT_VERSION_MINOR 0)
set(GFXRECONSTRUCT_PROJECT_VERSION_PATCH 5)

set(GFXRECON_PROJECT_VERSION_SHA1 "unknown-build-source")
set(GFXRECONSTRUCT_PROJECT_BUILD_TYPE "${CMAKE_BUILD_TYPE}")

include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
set(GIT_BRANCH "")

if (GIT_REFSPEC)
    string(REGEX REPLACE ".*/(.+)$" "\\1" GIT_BRANCH ${GIT_REFSPEC})
    string(COMPARE EQUAL ${GIT_BRANCH} "master" GIT_IS_MASTER)
    string(COMPARE EQUAL ${GIT_BRANCH} "release-internal" GIT_IS_RELEASE)
    string(REGEX MATCH "^vulkan-sdk-[0-9]+\.[0-9]+\.[0-9]+$" GIT_IS_SDK ${GIT_BRANCH})

    if (GIT_IS_MASTER OR GIT_IS_SDK OR GIT_IS_RELEASE)
        if (GIT_TAG)
            set(GIT_BRANCH ${GIT_TAG})
        else()
            set(GIT_BRANCH "")
        endif()
        if(NOT DEFINED GFXRECON_PROJECT_VERSION_DESIGNATION)
            set(GFXRECON_PROJECT_VERSION_DESIGNATION "-release-internal")
        endif()
    elseif(NOT DEFINED GFXRECON_PROJECT_VERSION_DESIGNATION)
        set(GFXRECON_PROJECT_VERSION_DESIGNATION "-dev")
    endif()
elseif(GIT_TAG)
    string(REGEX MATCH "^v[0-9]+\.[0-9]+\.[0-9]+$" GIT_IS_VERSION_RELEASE_TAG ${GIT_TAG})
    if (GIT_IS_VERSION_RELEASE_TAG)
        set(GIT_BRANCH ${GIT_TAG})
        set(GFXRECON_PROJECT_VERSION_DESIGNATION "")
    endif()
elseif(NOT DEFINED GFXRECON_PROJECT_VERSION_DESIGNATION)
    set(GFXRECON_PROJECT_VERSION_DESIGNATION "-unknown")
endif()

if (GIT_SHA1)
    string(SUBSTRING ${GIT_SHA1} 0 7 GFXRECON_PROJECT_VERSION_SHA1)

    if (GIT_BRANCH)
        string(CONCAT GFXRECON_PROJECT_VERSION_SHA1 ${GIT_BRANCH} ":" ${GFXRECON_PROJECT_VERSION_SHA1})
    endif()

    git_local_changes(GIT_LOCAL_STATE)
    string(COMPARE EQUAL ${GIT_LOCAL_STATE} "DIRTY" GIT_DIRTY)
    if (GIT_DIRTY)
        string(CONCAT GFXRECON_PROJECT_VERSION_SHA1 ${GFXRECON_PROJECT_VERSION_SHA1} "*")
    endif()
endif()

include(GenerateVersionARM)

# Adds all the configure time information into project_version_temp.h.in
configure_file("${GFXRECON_SOURCE_DIR}/project_version.h.in" "${CMAKE_BINARY_DIR}/project_version_temp.h.in")

# Generate a "project_version_$<CONFIG>.h" for the current config - necessary to determine the current build configuration
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/project_version_$<CONFIG>.h" INPUT "${CMAKE_BINARY_DIR}/project_version_temp.h.in")

# Since project_version_$<CONFIG>.h differs per build, set a compiler definition that files can use to include it
add_definitions(-DPROJECT_VERSION_HEADER_FILE="project_version_$<CONFIG>.h")

# Isolate the repo SHA in a library to reduce the amount of recompilation on git commit, checkout, etc.
# Clients should #include PROJECT_VERSION_HEADER_FILE and call GetProjectVersionString()
configure_file("${GFXRECON_SOURCE_DIR}/project_version_string.h.in" "${CMAKE_BINARY_DIR}/project_version_string.h")
add_library(project_version "${GFXRECON_SOURCE_DIR}/project_version.cpp")
target_include_directories(project_version PUBLIC "${CMAKE_BINARY_DIR}")
include(ApplyVulkanHeadersPatch)

# For now, Android requires Vulkan to be enabled
set(GFXRECON_ENABLE_VULKAN ON)
add_definitions(-DGFXRECON_ENABLE_VULKAN)

add_library(platform_specific INTERFACE)
target_compile_definitions(platform_specific INTERFACE
                                _FILE_OFFSET_BITS=64
                                PAGE_GUARD_ENABLE_UCONTEXT_WRITE_DETECTION
                                VK_USE_PLATFORM_ANDROID_KHR
                                XR_USE_PLATFORM_ANDROID
                                XR_USE_GRAPHICS_API_VULKAN)

add_library(vulkan_registry INTERFACE)
target_include_directories(vulkan_registry INTERFACE ${GFXRECON_SOURCE_DIR}/external/Vulkan-Headers/include)
target_compile_definitions(vulkan_registry INTERFACE VK_NO_PROTOTYPES VK_ENABLE_BETA_EXTENSIONS)

add_library(spirv_registry INTERFACE)
target_include_directories(spirv_registry INTERFACE ${GFXRECON_SOURCE_DIR}/external/SPIRV-Headers/include)

add_library(vulkan_memory_allocator INTERFACE)
target_include_directories(vulkan_memory_allocator INTERFACE ${GFXRECON_SOURCE_DIR}/external/VulkanMemoryAllocator/include)

# SPIRV-Reflect included as submodule -> libspirv-reflect-static.a
set(SPIRV_REFLECT_EXAMPLES OFF CACHE INTERNAL "no spirv_reflect samples" FORCE)
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE INTERNAL "no spirv_reflect executables" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE INTERNAL "create spirv-reflect-static library" FORCE)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW CACHE INTERNAL "set a cmake policy for spirv_reflect" FORCE)
add_subdirectory("${GFXRECON_SOURCE_DIR}/external/SPIRV-Reflect" EXCLUDE_FROM_ALL "${CMAKE_BINARY_DIR}/external/SPIRV-Reflect")
include_directories("${GFXRECON_SOURCE_DIR}/external/SPIRV-Reflect")

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

# No-op stub so shared framework CMakeLists.txt files can call this macro on
# Android without error.  The desktop root CMakeLists.txt provides the real
# implementation that wires up code-style checks, lint, and the test package.
macro(common_build_directives TARGET)
endmacro()
