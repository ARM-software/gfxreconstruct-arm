###############################################################################
# Copyright (c) 2025 LunarG, Inc.
# Copyright (c) 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

add_executable(gfxrecon-spirv-simulator "")

target_sources(gfxrecon-spirv-simulator
               PRIVATE
                   ${CMAKE_CURRENT_LIST_DIR}/simulator_test.cpp
                   ${CMAKE_CURRENT_LIST_DIR}/../optimize/file_optimizer.h
                   ${CMAKE_CURRENT_LIST_DIR}/../optimize/file_optimizer.cpp
                   ${CMAKE_CURRENT_LIST_DIR}/../optimize/vulkan_file_optimizer.h
                   ${CMAKE_CURRENT_LIST_DIR}/../optimize/vulkan_file_optimizer.cpp
                   ${CMAKE_CURRENT_LIST_DIR}/../platform_debug_helper.cpp
                   $<$<BOOL:WIN32>:${PROJECT_SOURCE_DIR}/version.rc>
              )

if (MSVC)
    # Force inclusion of "gfxrecon_disable_popup_result" variable in linking.
    # On 32-bit windows, MSVC prefixes symbols with "_" but on 64-bit windows it doesn't.
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
      target_link_options(gfxrecon-spirv-simulator PUBLIC "LINKER:/Include:_gfxrecon_disable_popup_result")
    else()
      target_link_options(gfxrecon-spirv-simulator PUBLIC "LINKER:/Include:gfxrecon_disable_popup_result")
    endif()
endif()

target_include_directories(gfxrecon-spirv-simulator PUBLIC ${CMAKE_BINARY_DIR} ${PROJECT_SOURCE_DIR}/external/spirv-simulator/)

target_link_libraries(gfxrecon-spirv-simulator
                          gfxrecon_application
                          gfxrecon_decode
                          gfxrecon_encode
                          gfxrecon_graphics
                          gfxrecon_format
                          gfxrecon_util
                          platform_specific
                          gfxrecon_vulkan_spirv_analysis)

common_build_directives(gfxrecon-spirv-simulator)

install(TARGETS gfxrecon-spirv-simulator RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
