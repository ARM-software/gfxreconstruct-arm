#!/usr/bin/python3 -i
#
# Copyright (c) 2024 LunarG, Inc.
# Copyright (c) 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>
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
#

import sys
from base_generator import BaseGenerator, BaseGeneratorOptions, write


class VulkanSkiavkModifierHeaderGeneratorOptions(BaseGeneratorOptions):
    """Adds the following new option:
    is_override - Specify whether the member function declarations are
                  virtual function overrides or pure virtual functions.
    Options for generating C++ class declarations for Vulkan parameter processing.
    """

    def __init__(
        self,
        class_name,
        base_class_header,
        is_override,
        constructor_args='',
        blacklists=None,  # Path to JSON file listing apicalls and structs to ignore.
        platform_types=None,  # Path to JSON file listing platform (WIN32, X11, etc.) defined types.
        filename=None,
        directory='.',
        prefix_text='',
        protect_file=False,
        protect_feature=True,
        extra_headers=[]
    ):
        BaseGeneratorOptions.__init__(
            self,
            blacklists,
            platform_types,
            filename,
            directory,
            prefix_text,
            protect_file,
            protect_feature,
            extra_headers=extra_headers
        )
        self.class_name = class_name
        self.base_class_header = base_class_header
        self.is_override = is_override
        self.constructor_args = constructor_args


class VulkanSkiavkModifierHeaderGenerator(BaseGenerator):
    """VulkanConsumerHeaderGenerator - subclass of BaseGenerator.
    Generates C++ member declarations for the VulkanConsumer class responsible for processing
    Vulkan API call parameter data.
    Generate C++ class declarations for Vulkan parameter processing.
    """

    def __init__(
        self, err_file=sys.stderr, warn_file=sys.stderr, diag_file=sys.stdout
    ):
        BaseGenerator.__init__(
            self,
            err_file=err_file,
            warn_file=warn_file,
            diag_file=diag_file
        )

    def beginFile(self, gen_opts):
        """Method override."""

        # Copyright text prefixing all headers (list of strings).
        arm_copyright_text = [
            '/*', '** Copyright (c) 2024 LunarG, Inc',
            '** Copyright (c) 2024 Arm Limited and/or its affiliates <open-source-office@arm.com>', '**',
            '** Permission is hereby granted, free of charge, to any person obtaining a',
            '** copy of this software and associated documentation files (the "Software"),',
            '** to deal in the Software without restriction, including without limitation',
            '** the rights to use, copy, modify, merge, publish, distribute, sublicense,',
            '** and/or sell copies of the Software, and to permit persons to whom the',
            '** Software is furnished to do so, subject to the following conditions:',
            '**',
            '** The above copyright notice and this permission notice shall be included in',
            '** all copies or substantial portions of the Software.', '**',
            '** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR',
            '** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,',
            '** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE',
            '** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER',
            '** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING',
            '** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER',
            '** DEALINGS IN THE SOFTWARE.', '*/', ''
        ]

        gen_opts.prefix_text = arm_copyright_text + gen_opts.prefix_text
        BaseGenerator.beginFile(self, gen_opts)

        write('#include "util/vulkan_modifier_base.h"', file=self.outFile)
        write('#include <unordered_map>', file=self.outFile)
        write('#include <unordered_set>', file=self.outFile)
        self.newline()

        write('GFXRECON_BEGIN_NAMESPACE(gfxrecon)', file=self.outFile)
        write('GFXRECON_BEGIN_NAMESPACE(decode)', file=self.outFile)
        self.newline()
        write(
            'class {class_name} : public util::VulkanModifierBase'.format(
                class_name=gen_opts.class_name
            ),
            file=self.outFile
        )
        write('{', file=self.outFile)
        write('  public:', file=self.outFile)
        write('    void SetAppName(std::vector<std::string> appNameArray) { VulkanSkiaModifier::app_name_array = appNameArray; }', file=self.outFile)
        write('    bool CanOptimize() override;', file=self.outFile)
        write('    void CheckSkiavk(format::HandleId vulkanHandle)', file=self.outFile)
        write('    {', file=self.outFile)
        write('        if (IsModificationPass()) return;', file=self.outFile)
        write('        if (IsSkiaBlock(vulkanHandle))', file=self.outFile)
        write('        {', file=self.outFile)
        write('            SetDeleteCurrentCall();', file=self.outFile)
        write('        }', file=self.outFile)
        write('    }', file=self.outFile)
        write('    virtual bool GetDeleteCurrentCall()', file=self.outFile)
        write('    {', file=self.outFile)
        write('        return (skiavkindex2remove.find(block_index_) != skiavkindex2remove.end());', file=self.outFile)
        write('    }', file=self.outFile)
        write('    void SetDeleteCurrentCall()', file=self.outFile)
        write('    {', file=self.outFile)
        write('        skiavkindex2remove[block_index_] = true;', file=self.outFile)
        write('    }', file=self.outFile)
        self.newline()
        write('  public: //vulkan function', file=self.outFile)
        if gen_opts.constructor_args:
            arg_list = ', '.join(
                [
                    arg.split(' ')[-1]
                    for arg in gen_opts.constructor_args.split(',')
                ]
            )
            write(
                '    {class_name}({}) : {class_name}Base({}) {{ }}\n'.format(
                    gen_opts.constructor_args,
                    arg_list,
                    class_name=gen_opts.class_name
                ),
                file=self.outFile
            )
        else:
            write(
                '    {}() {{ }}\n'.format(gen_opts.class_name),
                file=self.outFile
            )
        write(
            '    virtual ~{}() override {{ }}'.format(gen_opts.class_name),
            file=self.outFile
        )

    def endFile(self):
        """Method override."""
        self.newline()
        write('  public: // meta data function', file=self.outFile)
        write('    virtual void ProcessFillMemoryCommand(uint64_t memory_id, uint64_t offset, uint64_t size, const uint8_t* data);', file=self.outFile)
        write('    virtual void ProcessFixDeviceAddressCommand(const format::FixDeviceAddressCommandHeader& header,', file=self.outFile)
        write('                                                const format::AddressLocationInfo*           infos);', file=self.outFile)
        write('    virtual void ProcessFixShaderGroupHandleCommand(const format::FixShaderGroupHandleCommandHeader& header,', file=self.outFile)
        write('                                                    const format::ShaderHandleLocationInfo*          infos);', file=self.outFile)
        write('    virtual void ProcessResizeWindowCommand(format::HandleId surface_id, uint32_t width, uint32_t height);', file=self.outFile)
        write('    virtual void', file=self.outFile)
        write('    ProcessResizeWindowCommand2(format::HandleId surface_id, uint32_t width, uint32_t height, uint32_t pre_transform);', file=self.outFile)
        write('    virtual void ProcessCreateHardwareBufferCommand(format::HandleId                                memory_id,', file=self.outFile)
        write('                                                uint64_t                                            buffer_id,', file=self.outFile)
        write('                                                uint32_t                                            format,', file=self.outFile)
        write('                                                uint32_t                                            width,', file=self.outFile)
        write('                                                uint32_t                                            height,', file=self.outFile)
        write('                                                uint32_t                                            stride,', file=self.outFile)
        write('                                                uint64_t                                            usage,', file=self.outFile)
        write('                                                uint32_t                                            layers,', file=self.outFile)
        write('                                                const std::vector<format::HardwareBufferPlaneInfo>& plane_info);', file=self.outFile)
        write('    virtual void ProcessDestroyHardwareBufferCommand(uint64_t buffer_id);', file=self.outFile)
        write('    virtual void ProcessSetDevicePropertiesCommand(format::HandleId   physical_device_id,', file=self.outFile)
        write('                                               uint32_t           api_version,', file=self.outFile)
        write('                                               uint32_t           driver_version,', file=self.outFile)
        write('                                               uint32_t           vendor_id,', file=self.outFile)
        write('                                               uint32_t           device_id,', file=self.outFile)
        write('                                               uint32_t           device_type,', file=self.outFile)
        write('                                               const uint8_t      pipeline_cache_uuid[format::kUuidSize],', file=self.outFile)
        write('                                               const std::string& device_name);', file=self.outFile)
        write('    virtual void ProcessSetDeviceMemoryPropertiesCommand(format::HandleId physical_device_id,', file=self.outFile)
        write('                                                     const std::vector<format::DeviceMemoryType>& memory_types,', file=self.outFile)
        write('                                                     const std::vector<format::DeviceMemoryHeap>& memory_heaps);', file=self.outFile)
        write('    virtual void', file=self.outFile)
        write('    ProcessSetOpaqueAddressCommand(format::HandleId device_id, format::HandleId object_id, uint64_t address);', file=self.outFile)
        write('    virtual void ProcessSetRayTracingShaderGroupHandlesCommand(format::HandleId device_id,', file=self.outFile)
        write('                                                           format::HandleId pipeline_id,', file=self.outFile)
        write('                                                           size_t           data_size,', file=self.outFile)
        write('                                                           const uint8_t*   data);', file=self.outFile)
        write('    virtual void ProcessSetSwapchainImageStateCommand(format::HandleId device_id,', file=self.outFile)
        write('                                                  format::HandleId swapchain_id,', file=self.outFile)
        write('                                                  uint32_t         last_presented_image,', file=self.outFile)
        write('                                                  const std::vector<format::SwapchainImageStateInfo>& image_state);', file=self.outFile)
        write('    virtual void', file=self.outFile)
        write('    ProcessBeginResourceInitCommand(format::HandleId device_id, uint64_t max_resource_size, uint64_t max_copy_size);', file=self.outFile)
        write('    virtual void ProcessEndResourceInitCommand(format::HandleId device_id);', file=self.outFile)
        write('    virtual void ProcessInitBufferCommand(format::HandleId device_id,', file=self.outFile)
        write('                                      format::HandleId buffer_id,', file=self.outFile)
        write('                                      uint64_t         data_size,', file=self.outFile)
        write('                                      const uint8_t*   data);', file=self.outFile)
        write('    virtual void ProcessInitImageCommand(format::HandleId             device_id,', file=self.outFile)
        write('                                     format::HandleId             image_id,', file=self.outFile)
        write('                                     uint64_t                     data_size,', file=self.outFile)
        write('                                     uint32_t                     aspect,', file=self.outFile)
        write('                                     uint32_t                     layout,', file=self.outFile)
        write('                                     const std::vector<uint64_t>& level_sizes,', file=self.outFile)
        write('                                     const uint8_t*               data);', file=self.outFile)
        write('    virtual void ProcessInitSubresourceCommand(const format::InitSubresourceCommandHeader& command_header,', file=self.outFile)
        write('                                           const uint8_t*                              data);', file=self.outFile)
        write('    virtual void ProcessBuildVulkanAccelerationStructuresMetaCommand(', file=self.outFile)
        write('    format::HandleId                                                           device_id,', file=self.outFile)
        write('    uint32_t                                                                   info_count,', file=self.outFile)
        write('    StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* geometry_infos,', file=self.outFile)
        write('    StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   range_infos,', file=self.outFile)
        write('    std::vector<std::vector<VkAccelerationStructureInstanceKHR>>&              instance_buffers_data);', file=self.outFile)
        write('    virtual void ProcessCopyVulkanAccelerationStructuresMetaCommand(', file=self.outFile)
        write('    format::HandleId device_id, StructPointerDecoder<Decoded_VkCopyAccelerationStructureInfoKHR>* copy_infos);', file=self.outFile)
        write('    virtual void ProcessVulkanAccelerationStructuresWritePropertiesMetaCommand(', file=self.outFile)
        write('    format::HandleId device_id, VkQueryType query_type, format::HandleId acceleration_structure_id);', file=self.outFile)
        write('    virtual void ProcessFrameEndMarker(uint64_t frame_number) override;', file=self.outFile)
        self.newline()
        write('  private:', file=self.outFile)
        write('    bool IsSkiaBlock(format::HandleId handle);', file=self.outFile)
        self.newline()
        write('  private:', file=self.outFile)
        write('    bool                                                                not_skiavk_instance = false;', file=self.outFile)
        write('    bool                                                                skiavk_instance     = false;', file=self.outFile)
        write('    std::vector<uint64_t>                                               frames_to_be_removed;', file=self.outFile)
        write('    static std::vector<std::string>                                     app_name_array;', file=self.outFile)
        write('    std::unordered_map<uint64_t, bool>                                  skiavkindex2remove;', file=self.outFile)
        write('    std::unordered_map<format::HandleId, std::vector<format::HandleId>> skiavk_instance2physical_device;', file=self.outFile)
        write('    std::unordered_map<format::HandleId, std::vector<format::HandleId>> skia_instance2surface;', file=self.outFile)
        write('    std::unordered_map<format::HandleId, std::vector<format::HandleId>> skiavk_physical_device2device;', file=self.outFile)
        write('    std::unordered_map<format::HandleId, std::vector<format::HandleId>> skia_device2queue;', file=self.outFile)
        write('    std::unordered_map<format::HandleId, std::vector<format::HandleId>> skia_device2command_pool;', file=self.outFile)
        write('    std::unordered_map<format::HandleId, std::vector<format::HandleId>> skia_command_pool2command_buffer;', file=self.outFile)
        write('    std::unordered_map<format::HandleId, std::vector<format::HandleId>> skia_device2memory;', file=self.outFile)
        write('    std::unordered_map<format::HandleId, std::vector<format::HandleId>> skia_device2buffer;', file=self.outFile)
        write('};', file=self.outFile)
        self.newline()
        write('GFXRECON_END_NAMESPACE(decode)', file=self.outFile)
        write('GFXRECON_END_NAMESPACE(gfxrecon)', file=self.outFile)

        # Finish processing in superclass
        BaseGenerator.endFile(self)

    #
    # Indicates that the current feature has C++ code to generate.
    def need_feature_generation(self):
        if self.feature_cmd_params:
            return True
        return False

    def generate_feature(self):
        """Performs C++ code generation for the feature."""
        api_exclusions = [
                'vkCreateInstance',
                'vkDestroyInstance',
                'vkEnumeratePhysicalDevices',
                'vkCreateDevice',
                'vkDestroyDevice',
                'vkGetDeviceQueue',
                'vkGetDeviceQueue2',
                'vkCreateCommandPool',
                'vkDestroyCommandPool',
                'vkAllocateCommandBuffers',
                'vkFreeCommandBuffers',
                'vkAllocateMemory',
                'vkFreeMemory',
                'vkCreateAndroidSurfaceKHR',
                'vkCreateBuffer',
                'vkDestroyBuffer',
                'vkEnumeratePhysicalDevices'
                ]
        first = True
        for cmd in self.get_filtered_cmd_names():
            info = self.feature_cmd_params[cmd]
            return_type = info[0]
            values = info[2]
            decl = self.make_consumer_func_decl(
                return_type, 'Process_' + cmd, values
            )
            param_decls = []
            param_decl = self.make_aligned_param_decl(
                'const ApiCallInfo&', 'call_info', self.INDENT_SIZE,
                self.genOpts.align_func_param
            )

            cmddef = '' if first else '\n'
            if self.genOpts.is_override:
                cmddef += self.indent(
                    'virtual ' + decl + ' override;', self.INDENT_SIZE
                )
            else:
                if cmd in api_exclusions:
                    cmddef += self.indent(
                        'virtual ' + decl + ';', self.INDENT_SIZE
                    )
                else:
                    for value in values:
                        param_type = self.make_decoded_param_type(value)
                        if param_type == 'format::HandleId':
                            break
                    cmddef += self.indent(
                        'virtual ' + decl + '{ CheckSkiavk(%s);}' % value.name, self.INDENT_SIZE
                    )

            write(cmddef, file=self.outFile)
            first = False
