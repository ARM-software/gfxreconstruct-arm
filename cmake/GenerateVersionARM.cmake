include_guard(GLOBAL)

set(GFXRECONSTRUCT_PROJECT_ARM_VERSION_MAJOR 4)
set(GFXRECONSTRUCT_PROJECT_ARM_VERSION_MINOR 4)

get_cmake_property(_variableNames VARIABLES)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake" "${CMAKE_CURRENT_LIST_DIR}/../external/cmake-modules")
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
set(GIT_BRANCH "")
set(GFXRECON_PROJECT_VERSION_DESIGNATION "$ENV{GFXRECON_PROJECT_VERSION_DESIGNATION}")

git_local_changes(GIT_LOCAL_STATE)
string(COMPARE EQUAL ${GIT_LOCAL_STATE} "DIRTY" GIT_DIRTY)

## Generate short SHA string
string(SUBSTRING "${GIT_SHA1}" 0 8 GIT_SHA1_SHORT)
if (GIT_DIRTY)
    string(CONCAT GIT_SHA1_SHORT ${GIT_SHA1_SHORT} "*")
endif ()

if (GIT_REFSPEC)
    string(REGEX REPLACE ".*/(.+)$" "\\1" GIT_BRANCH ${GIT_REFSPEC})
    ## Skip designation information on release branch, otherwise set it to:
    ## <branch name>-<sha><"*" if local changes>
    if (NOT GIT_BRANCH MATCHES "^release-.*$")
        string(APPEND GFXRECON_PROJECT_VERSION_DESIGNATION " ${GIT_BRANCH}:${GIT_SHA1_SHORT}")
    endif ()
else ()
## GIT_REFSPEC may not exist in detached state - use "DETACHED" branch name and GIT_SHA1_SHORT as designation
    string(APPEND GFXRECON_PROJECT_VERSION_DESIGNATION " DETACHED:${GIT_SHA1_SHORT}")
endif ()

# Build type may come from CMAKE_BUILD_TYPE or - for gradle builds - from command line
set(BUILD_TYPE "")

# If CMAKE_BUILD_TYPE is set, override BUILD_TYPE with its value
if(DEFINED CMAKE_BUILD_TYPE AND NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(BUILD_TYPE "${CMAKE_BUILD_TYPE}")
else()
    # Parse -BUILD_TYPE=... from script arguments
    math(EXPR _lastArg "${CMAKE_ARGC} - 1")
    foreach(_i RANGE 0 ${_lastArg})
        if(CMAKE_ARGV${_i} MATCHES "^-BUILD_TYPE=(.*)$")
            set(BUILD_TYPE "${CMAKE_MATCH_1}")
        endif()
    endforeach()
endif()

math(EXPR GFXRECONSTRUCT_VERSION_INT_ARM "${GFXRECONSTRUCT_PROJECT_ARM_VERSION_MAJOR} << 12 | ${GFXRECONSTRUCT_PROJECT_ARM_VERSION_MINOR}")
set(GFXRECONSTRUCT_VERSION_STRING_ARM "ARM r${GFXRECONSTRUCT_PROJECT_ARM_VERSION_MAJOR}p${GFXRECONSTRUCT_PROJECT_ARM_VERSION_MINOR}${GFXRECON_PROJECT_VERSION_DESIGNATION} ${BUILD_TYPE}")

message(STATUS "GFXRECON_VERSION_STRING_ARM=${GFXRECONSTRUCT_VERSION_STRING_ARM}")
message(STATUS "GFXRECON_VERSION_INT_ARM=${GFXRECONSTRUCT_VERSION_INT_ARM}")