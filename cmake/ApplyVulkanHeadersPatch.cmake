option(GFXRECON_APPLY_VULKAN_HEADERS_PATCH "Apply Vulkan-Headers.patch to external/Vulkan-Headers during configure" ON)

set(GFXRECON_VULKAN_HEADERS_PATCH_FILE
    "${GFXRECON_SOURCE_DIR}/Vulkan-Headers.patch"
    CACHE FILEPATH
    "Patch to apply to external/Vulkan-Headers during configure")

if (GFXRECON_APPLY_VULKAN_HEADERS_PATCH)
    set(GFXRECON_VULKAN_HEADERS_DIR "${GFXRECON_SOURCE_DIR}/external/Vulkan-Headers")

    if (EXISTS "${GFXRECON_VULKAN_HEADERS_PATCH_FILE}")
        if (NOT EXISTS "${GFXRECON_VULKAN_HEADERS_DIR}")
            message(FATAL_ERROR "Cannot apply ${GFXRECON_VULKAN_HEADERS_PATCH_FILE}: ${GFXRECON_VULKAN_HEADERS_DIR} does not exist")
        endif ()

        find_package(Git QUIET)
        if (NOT GIT_FOUND)
            message(FATAL_ERROR "Git is required to apply ${GFXRECON_VULKAN_HEADERS_PATCH_FILE}")
        endif ()

        # Builds may configure several modules concurrently, so serialize
        # updates to their shared Vulkan-Headers working tree.
        set(GFXRECON_VULKAN_HEADERS_PATCH_LOCK_FILE
            "${GFXRECON_SOURCE_DIR}/._ApplyVulkanHeadersPatch.lock")
        file(LOCK
            "${GFXRECON_VULKAN_HEADERS_PATCH_LOCK_FILE}"
            TIMEOUT 30
            RESULT_VARIABLE GFXRECON_VULKAN_HEADERS_PATCH_LOCK_RESULT)
        if (NOT GFXRECON_VULKAN_HEADERS_PATCH_LOCK_RESULT STREQUAL "0")
            message(FATAL_ERROR
                "Failed to lock ${GFXRECON_VULKAN_HEADERS_PATCH_LOCK_FILE}: "
                "${GFXRECON_VULKAN_HEADERS_PATCH_LOCK_RESULT}")
        endif ()

        execute_process(
            COMMAND ${GIT_EXECUTABLE} apply --check -p3 "${GFXRECON_VULKAN_HEADERS_PATCH_FILE}"
            WORKING_DIRECTORY "${GFXRECON_VULKAN_HEADERS_DIR}"
            RESULT_VARIABLE GFXRECON_VULKAN_HEADERS_PATCH_APPLIES
            OUTPUT_VARIABLE GFXRECON_VULKAN_HEADERS_PATCH_CHECK_OUTPUT
            ERROR_VARIABLE GFXRECON_VULKAN_HEADERS_PATCH_CHECK_ERROR)

        if (GFXRECON_VULKAN_HEADERS_PATCH_APPLIES EQUAL 0)
            execute_process(
                COMMAND ${GIT_EXECUTABLE} apply -p3 "${GFXRECON_VULKAN_HEADERS_PATCH_FILE}"
                WORKING_DIRECTORY "${GFXRECON_VULKAN_HEADERS_DIR}"
                RESULT_VARIABLE GFXRECON_VULKAN_HEADERS_PATCH_RESULT
                OUTPUT_VARIABLE GFXRECON_VULKAN_HEADERS_PATCH_OUTPUT
                ERROR_VARIABLE GFXRECON_VULKAN_HEADERS_PATCH_ERROR)

            if (NOT GFXRECON_VULKAN_HEADERS_PATCH_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to apply ${GFXRECON_VULKAN_HEADERS_PATCH_FILE}: ${GFXRECON_VULKAN_HEADERS_PATCH_ERROR}")
            endif ()

            message(STATUS "Applied ${GFXRECON_VULKAN_HEADERS_PATCH_FILE} to external/Vulkan-Headers")
        else ()
            execute_process(
                COMMAND ${GIT_EXECUTABLE} apply --reverse --check -p3 "${GFXRECON_VULKAN_HEADERS_PATCH_FILE}"
                WORKING_DIRECTORY "${GFXRECON_VULKAN_HEADERS_DIR}"
                RESULT_VARIABLE GFXRECON_VULKAN_HEADERS_PATCH_REVERSES
                OUTPUT_VARIABLE GFXRECON_VULKAN_HEADERS_PATCH_REVERSE_OUTPUT
                ERROR_VARIABLE GFXRECON_VULKAN_HEADERS_PATCH_REVERSE_ERROR)

            if (GFXRECON_VULKAN_HEADERS_PATCH_REVERSES EQUAL 0)
                message(STATUS "${GFXRECON_VULKAN_HEADERS_PATCH_FILE} is already applied to external/Vulkan-Headers")
            else ()
                message(FATAL_ERROR
                    "${GFXRECON_VULKAN_HEADERS_PATCH_FILE} cannot be applied cleanly to external/Vulkan-Headers.\n"
                    "Forward check error:\n${GFXRECON_VULKAN_HEADERS_PATCH_CHECK_ERROR}\n"
                    "Reverse check error:\n${GFXRECON_VULKAN_HEADERS_PATCH_REVERSE_ERROR}")
            endif ()
        endif ()

        file(LOCK "${GFXRECON_VULKAN_HEADERS_PATCH_LOCK_FILE}" RELEASE)
    endif ()
endif ()
