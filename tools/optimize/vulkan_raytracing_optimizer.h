#ifndef GFXRECON_TOOLS_OPTIMIZE_VULKAN_RAYTRACING_OPTIMIZER_H
#define GFXRECON_TOOLS_OPTIMIZE_VULKAN_RAYTRACING_OPTIMIZER_H

#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#include "decode/referenced_resource_table.h"
#include "generated/generated_vulkan_consumer.h"
#include "util/defines.h"
#include "util/memory_output_stream.h"
#include "encode/parameter_buffer.h"
#include "vulkan/vulkan.h"
#include "decode/vulkan_object_info.h"
#include "util/vulkan_modifier_base.h"
#include "encode/struct_pointer_encoder.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

// Performs optimization of raytracing content
// In the first pass tracks memory modifications in order to indentify memory ranges containing device addresses and
// shader group handles In second pass injects FixDeviceAddress metacommand instructing the replayer to replace memory
// range with a device address or shader group handle value
class VulkanRaytracingOptimizer : public util::VulkanModifierBase
{
  protected:
    // forward declarations
    struct MemoryAllocation;
    class MemoryModification;
    class CommandBuffer;
    class MemoryAllocation;
    class Buffer;
    class BindBufferMemory;
    class IndexBufferData;
    struct BufferRange;
    class Call;

  public:
    VulkanRaytracingOptimizer() = default;

    virtual bool CanOptimize() override;

    virtual void Process_vkGetRayTracingShaderGroupHandlesKHR(const ApiCallInfo&       call_info,
                                                              VkResult                 returnValue,
                                                              format::HandleId         device,
                                                              format::HandleId         pipeline,
                                                              uint32_t                 firstGroup,
                                                              uint32_t                 groupCount,
                                                              size_t                   dataSize,
                                                              PointerDecoder<uint8_t>* pData) override;
    virtual void Process_vkCmdTraceRaysKHR(
        const ApiCallInfo&                                             call_info,
        format::HandleId                                               commandBuffer,
        StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pRaygenShaderBindingTable,
        StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pMissShaderBindingTable,
        StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pHitShaderBindingTable,
        StructPointerDecoder<Decoded_VkStridedDeviceAddressRegionKHR>* pCallableShaderBindingTable,
        uint32_t                                                       width,
        uint32_t                                                       height,
        uint32_t                                                       depth) override;

    virtual void
    ProcessFillMemoryCommand(uint64_t memory_id, uint64_t offset, uint64_t size, const uint8_t* data) override;

    virtual void Process_vkCmdUpdateBuffer(const ApiCallInfo&       call_info,
                                           format::HandleId         commandBuffer,
                                           format::HandleId         dstBuffer,
                                           VkDeviceSize             dstOffset,
                                           VkDeviceSize             dataSize,
                                           PointerDecoder<uint8_t>* pData) override;

    virtual void Process_vkQueueSubmit(const ApiCallInfo&                          call_info,
                                       VkResult                                    returnValue,
                                       format::HandleId                            queue,
                                       uint32_t                                    submitCount,
                                       StructPointerDecoder<Decoded_VkSubmitInfo>* pSubmits,
                                       format::HandleId                            fence) override;

    // Record FixDeviceAddressCommand for future reference
    // Fix/FillMemory commands might happen at any time, so this data may be processed only on submission
    virtual void ProcessFixDeviceAddressCommand(const format::FixDeviceAddressCommandHeader& header,
                                                const format::AddressLocationInfo*           infos) override;

    virtual void ProcessFixShaderGroupHandleCommand(const format::FixShaderGroupHandleCommandHeader& header,
                                                    const format::ShaderHandleLocationInfo*          infos) override;

    virtual void
    Process_vkBeginCommandBuffer(const ApiCallInfo&                                      call_info,
                                 VkResult                                                returnValue,
                                 format::HandleId                                        commandBuffer,
                                 StructPointerDecoder<Decoded_VkCommandBufferBeginInfo>* pBeginInfo) override;

    // Record BindBufferMemory for future reference.
    // Buffers may be bound to memory at any time and FillMemory might have happened before or after binding.
    // Binding data needs to be recorded here for processing on QueueSubmit

    virtual void Process_vkBindBufferMemory(const ApiCallInfo& call_info,
                                            VkResult           returnValue,
                                            format::HandleId   device,
                                            format::HandleId   buffer,
                                            format::HandleId   memory,
                                            VkDeviceSize       memoryOffset) override;

    // Add new draw command data for this cmdbuffer
    virtual void Process_vkCmdDrawIndexed(const ApiCallInfo& call_info,
                                          format::HandleId   commandBuffer,
                                          uint32_t           indexCount,
                                          uint32_t           instanceCount,
                                          uint32_t           firstIndex,
                                          int32_t            vertexOffset,
                                          uint32_t           firstInstance) override;

    // Store index buffer binding data for the command buffer
    // Complete range of index data is not known, size is not specified at this point - filled in draw command
    virtual void Process_vkCmdBindIndexBuffer(const ApiCallInfo& call_info,
                                              format::HandleId   commandBuffer,
                                              format::HandleId   buffer,
                                              VkDeviceSize       offset,
                                              VkIndexType        indexType) override;

    virtual void
    Process_vkGetBufferDeviceAddress(const ApiCallInfo&                                       call_info,
                                     VkDeviceAddress                                          returnValue,
                                     format::HandleId                                         device,
                                     StructPointerDecoder<Decoded_VkBufferDeviceAddressInfo>* pInfo) override;

    virtual void Process_vkCreateBuffer(const ApiCallInfo&                                   call_info,
                                        VkResult                                             returnValue,
                                        format::HandleId                                     device,
                                        StructPointerDecoder<Decoded_VkBufferCreateInfo>*    pCreateInfo,
                                        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                        HandlePointerDecoder<VkBuffer>*                      pBuffer) override;

    virtual void Process_vkAllocateMemory(const ApiCallInfo&                                   call_info,
                                          VkResult                                             returnValue,
                                          format::HandleId                                     device,
                                          StructPointerDecoder<Decoded_VkMemoryAllocateInfo>*  pAllocateInfo,
                                          StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                          HandlePointerDecoder<VkDeviceMemory>*                pMemory) override;

    virtual void Process_vkCmdCopyBuffer(const ApiCallInfo&                          call_info,
                                         format::HandleId                            commandBuffer,
                                         format::HandleId                            srcBuffer,
                                         format::HandleId                            dstBuffer,
                                         uint32_t                                    regionCount,
                                         StructPointerDecoder<Decoded_VkBufferCopy>* pRegions) override;

    virtual void Process_vkCmdBuildAccelerationStructuresKHR(
        const ApiCallInfo&                                                         call_info,
        format::HandleId                                                           commandBuffer,
        uint32_t                                                                   infoCount,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildGeometryInfoKHR>* pInfos,
        StructPointerDecoder<Decoded_VkAccelerationStructureBuildRangeInfoKHR*>*   ppBuildRangeInfos) override;

  protected:
    // Multiple buffers can be bound to the same or overlapping areas of memory, so there can be multiple buffers
    // containing given device address This function returns all such buffers, except for those that never had its
    // device address retrieved current_index can be set to filter out deleted buffers
    std::vector<Buffer*> GetBufferByDeviceAddress(VkDeviceAddress address, uint64_t current_index = 0);

    // Traverses modification chain to identify the original modification source call - usually FillMemoryCommand
    Call* TrackSourceModification(Call* start_call, Buffer* first_modified_buffer, uint64_t offset, uint64_t size);

    // Prepares gathered data for analysis
    // Fills out missing information in collected data based on full trace data
    void ProcessIntermediateData();

    //  For each buffer, go over set bindings:
    //      vkCmdBindIndexBuffer
    //      vkCmdBindVertexBuffers
    //      vkCmdBindDescriptorSets + vkUpdateDescriptorSets
    //  For each FixDeviceAddres before submission, if any of these updates
    //  buffers bound as index or vertex, that location update should be removed
    void ValidateFixDeviceAddressCommands();

    void ValidateBuildAccelerationStructureCalls();

    // Every FillMemoryCommand was scanned for known values of shader group handles, buffer and acceleration structure
    // device addresses Matching instances were tracked, now they need to be verified
    void ProcessRuntimeVariables();

    // Each trace rays call contains a set of device addresses of shader binding tables
    // Attempt to track these addresses to the original call inserting shader grup handles at those addresses
    void ProcessCmdTraceRays();

  protected:

    // Represents generic block in the trace - a vulkan command or a metacommand
    class Call
    {
      public:
        enum class CallType
        {
            FillMemoryCommand,
            QueueSubmit,
            CmdCopyBuffer,
            CmdTraceRays,
            FixDeviceAddress,
            BindBufferMemory,
            CmdBuildAccelerationStructures,
            CmdBindIndexBuffer,
            CmdDrawIndexed,
        };
        Call(uint64_t index, CallType call_type) : index(index), call_type(call_type) {}
        virtual bool IsCmdCall() { return false; }

        uint64_t index;
        CallType call_type;

        // unique id for allowing to define an execution order that includes calls in CommandBuffers
        struct UniqueCallIndex
        {
            // for Cmd calls this is index of QueueSubmit call, and index for other calls
            uint64_t submit_index;
            // command buffer handle, for uniqueness within submissions, required by std::set
            format::HandleId command_buffer;
            // index of command inside command buffer
            uint64_t command_index;

            // attempt to order call executions
            bool operator<(const UniqueCallIndex& other) const
            {
                // Calls might be submitted in the same command buffer, in that case compare calls using their index inside command buffer
                if (other.submit_index == submit_index && other.command_buffer == command_buffer)
                {
                    return command_index < other.command_index;
                }
                else
                {
                  // use submit index if its not a cmd call or a cmd call from different command buffer
                    return submit_index < other.submit_index;
                }
            }
        };

        bool operator<(const Call& other) const { return GetUniqueCallIndex() < other.GetUniqueCallIndex(); }
        virtual UniqueCallIndex GetUniqueCallIndex() const { return { index, 0, 0 }; }
    };

    class QueueSubmitCall : public Call
    {
      public:
        QueueSubmitCall(uint64_t index) : Call(index, CallType::QueueSubmit) {}
        std::vector<CommandBuffer> command_buffers;
    };

    class CommandCall : public Call
    {
      public:
        CommandCall(uint64_t index, CallType call_type, format::HandleId command_buffer_handle) :
            Call(index, call_type), command_buffer_handle(command_buffer_handle)
        {}
        virtual bool IsCmdCall() { return true; }

        // for the purpose of ordering calls in actual execution order, assume this call index is actually the index of
        // the submit call containing the command buffer with this call
        virtual UniqueCallIndex GetUniqueCallIndex() const override
        {
            return { command_buffer_instance->submission->index, command_buffer_handle, command_index };
        }
        // index of command inside command buffer
        uint64_t         command_index;
        format::HandleId command_buffer_handle;
        CommandBuffer*   command_buffer_instance;
    };

    class CmdCopyBuffer : public CommandCall
    {
      public:
        CmdCopyBuffer(uint64_t index, format::HandleId command_buffer_handle) :
            CommandCall(index, CallType::CmdCopyBuffer, command_buffer_handle)
        {}
        Buffer*                   source;
        Buffer*                   destination;
        std::vector<VkBufferCopy> regions;
    };

    class FillMemoryCommand : public Call
    {
      public:
        FillMemoryCommand(uint64_t index) : Call(index, CallType::FillMemoryCommand) {}
        MemoryModification*   memory_modification;
        std::vector<uint64_t> offsets;
    };

    class CmdTraceRays : public CommandCall
    {
      public:
        class ExtendedStridedDeviceAddressRegion : public VkStridedDeviceAddressRegionKHR
        {
          public:
            // Shader Binding Table buffer pointed by device address region
            // This buffer is expected to contain shader group handles
            Buffer* sbt_buffer;
            ExtendedStridedDeviceAddressRegion(VkStridedDeviceAddressRegionKHR region) :
                VkStridedDeviceAddressRegionKHR(region)
            {}
        };
        ExtendedStridedDeviceAddressRegion raygen;
        ExtendedStridedDeviceAddressRegion miss;
        ExtendedStridedDeviceAddressRegion hit;
        ExtendedStridedDeviceAddressRegion callable;

        CmdTraceRays(uint64_t                         index,
                     format::HandleId                 command_buffer_handle,
                     VkStridedDeviceAddressRegionKHR* raygen,
                     VkStridedDeviceAddressRegionKHR* miss,
                     VkStridedDeviceAddressRegionKHR* hit,
                     VkStridedDeviceAddressRegionKHR* callable) :
            CommandCall(index, CallType::CmdTraceRays, command_buffer_handle),
            raygen(*raygen), miss(*miss), hit(*hit), callable(*callable)
        {}
    };

    // Records FixDeviceAddress command details.
    class FixDeviceAddress : public Call
    {
      public:
        FixDeviceAddress(uint64_t index) : Call(index, CallType::FixDeviceAddress) {}
        // modified memory id
        format::HandleId memory_id;
        // vector of modified locations
        std::vector<format::AddressLocationInfo> locations;
        VkDeviceSize                             fill_memory_data_offset_;
    };

    // BufferRange size is not always known
    class BufferRange
    {
      public:
        Buffer*      buffer;
        VkDeviceSize offset;
        VkDeviceSize size;
    };

    class IndexBufferData : public BufferRange
    {
      public:
        VkIndexType index_type;
    };

    class DrawIndexedCall : public CommandCall
    {
      public:
        DrawIndexedCall(uint64_t index, format::HandleId command_buffer) : CommandCall(index,  CallType::CmdDrawIndexed, command_buffer) {}
        IndexBufferData index_buffer;
        uint32_t        index_count;
        uint32_t        first_index;
    };

    class BindBufferMemory : public Call
    {
      public:
        BindBufferMemory(uint64_t index, Buffer* buffer, MemoryAllocation* memory, VkDeviceSize offset) :
            Call(index, CallType::BindBufferMemory), buffer(buffer), memory(memory), offset(offset)
        {}
        Buffer*           buffer;
        MemoryAllocation* memory;
        VkDeviceSize      offset;
    };

    class VulkanObject
    {
      public:
        VulkanObject(format::HandleId handle, VkObjectType type) : handle(handle), type(type) {}
        // Some objects require processing once the first pass is finished
        virtual void ProcessIntermediateData() {}
        format::HandleId handle;
        VkObjectType     type;
        uint64_t         creation_call;
        uint64_t         destruction_call;
    };

    class CommandBuffer : public VulkanObject
    {
      public:
        CommandBuffer() : VulkanObject(0, VK_OBJECT_TYPE_UNKNOWN) {}
        CommandBuffer(format::HandleId handle, VkObjectType type) : VulkanObject(handle, type) {}
        std::vector<CommandCall*> calls;
        QueueSubmitCall*          submission;

        // helper containers for fast traversal
        std::vector<DrawIndexedCall*>    draw_indexed_commands;
        std::vector<CmdTraceRays*>       trace_rays_commands;
        std::vector<CmdCopyBuffer*>      copy_commands;
        std::vector<MemoryModification*> memory_modifications;

        // Holds details of latest CmdBindIndexBuffer command
        IndexBufferData current_bound_index_buffer;

        void Reset(){
          calls.clear();
          submission = nullptr;
          draw_indexed_commands.clear();
          trace_rays_commands.clear();
          copy_commands.clear();
          memory_modifications.clear();
        }
    };

    class Buffer : public VulkanObject
    {
      public:
        Buffer(format::HandleId handle, VkDeviceSize size) : VulkanObject(handle, VK_OBJECT_TYPE_BUFFER), size(size) {}

        VkDeviceAddress   device_address;
        uint64_t          size;
        BindBufferMemory* memory_binding;
        // maps modification call index to modification calls
        // Modification calls are collected in a vector to cover calls from command buffers
        // in that case the maps key corresponds to the index of submit call
        std::map<uint64_t, std::vector<Call*>>        memory_updates;
        std::map<uint64_t, std::vector<CommandCall*>> buffer_updates;
    };

    class MemoryRange
    {
      public:
        VkDeviceSize      start_offset_;
        VkDeviceSize      end_offset_;
    };

    // Generic class representing *some* usage of a memory renage in a call
    // Usage could be a memory modificaiton, index buffer binding, acceleration structure storage etc.
    class MemoryRangeReference : public MemoryRange
    {
      public:
        Call*             call_;
        MemoryAllocation* memory_;

        MemoryRangeReference(Call* call, MemoryAllocation* memory, VkDeviceSize start, VkDeviceSize end)
        {
            call_         = call;
            memory_       = memory;
            start_offset_ = start;
            end_offset_   = end;
        }

        bool operator<(const MemoryRangeReference& other) const { return (*call_) < (*other.call_); }

        struct SortByExecutionOrder
        {
            bool operator()(const MemoryRangeReference& left, const MemoryRangeReference& right)
            {
                auto left_index  = left.call_->GetUniqueCallIndex();
                auto right_index = right.call_->GetUniqueCallIndex();
                return left_index < right_index;
            }
        };
    };

    class MemoryModification : public MemoryRangeReference
    {
      public:
        MemoryModification(Call* call, MemoryAllocation* memory, VkDeviceSize start, VkDeviceSize end) :
            MemoryRangeReference(call, memory, start, end)
        {}
        // Helper used for copy commands, this refers to source memory range
        MemoryModification* source_modification;

        std::vector<MemoryRange> device_addresses_;
        std::vector<MemoryRange> shader_group_handles_;
    };

    // Represents detected DeviceAddresses and ShaderGroupHandles in calls, like FillMemoryCommands
    class RuntimeVariableInstance : public MemoryRangeReference
    {
      public:

    };

    class MemoryAllocation : public VulkanObject
    {
      public:
        MemoryAllocation(format::HandleId handle, VkDeviceSize size) :
            VulkanObject(handle, VK_OBJECT_TYPE_DEVICE_MEMORY), size(size)
        {}

        virtual void ProcessIntermediateData() override
        {
            // Memory modificaion data should be sorted in order of execution, not index.
            // Note: Order of execution is not known until submission, so using sorted container like set doesn't really work here
            // TODO: Order of submission should also consider synchronization
            std::sort(modified_memory_ranges.begin(), modified_memory_ranges.end());
        }

        VkDeviceSize                   size;
        std::vector<BindBufferMemory*> bound_buffers;

        // contains memory modifications tracked in first pass
        std::vector<MemoryModification> modified_memory_ranges;

        // holds range of memory and a call that uses it for specific purpose - index data, vertex data, acceleration
        // structure etc.
        std::vector<MemoryRangeReference> memory_range_usage_;

        std::unordered_set<BindBufferMemory*> GetBoundBuffersByOffset(VkDeviceSize offset) const
        {
            std::unordered_set<BindBufferMemory*> result;
            for (const auto& buffer_binding : bound_buffers)
            {
                if (offset ==
                    std::clamp(offset, buffer_binding->offset, buffer_binding->offset + buffer_binding->buffer->size))
                {
                    result.insert(buffer_binding);
                }
            }
            return result;
        }
    };

    // Represents descriptors bound to the pipeline, accounting for the dynamic offset
    struct DescriptorBinding
    {
        format::HandleId      descriptor_handle;
        std::vector<uint32_t> dynamic_offsets;
    };

    // Represents descriptor data from descriptor writes
    struct DescriptorData
    {
        uint32_t         binding;
        VkDescriptorType type;
        format::HandleId buffer;
        uint32_t         buffer_offset;
        uint32_t         buffer_range;
    };

    struct InstanceGeometryAddresses
    {
        InstanceGeometryAddresses(CommandCall* call, VkDeviceAddress address) :
            cmd_build_as_index(call), address(address)
        {}
        // build command containing instance address
        CommandCall*    cmd_build_as_index;
        VkDeviceAddress address;
    };

    // Contains all tracked call data
    std::map<uint64_t, std::unique_ptr<Call>> calls_;

    // Contains all object data
    std::map<format::HandleId, std::unique_ptr<VulkanObject>> objects_;

    // Helper map with current command buffer state
    // Updated during first pass, cleared on every Reset, copied inside QueueSubmit objects on submit commands
    // Not to be used as soon as first pass is done or in CanOptimize()
    std::unordered_map<format::HandleId, CommandBuffer> command_buffers_;

    // List of calls where a parameter matched known runtime variable (device address/shader group handle)
    std::vector<Call*> set_runtime_variable_calls_;

    // Collects device addresses queried by GetBufferDeviceAddress calls
    std::map<VkDeviceAddress, Buffer*> buffer_device_addresses_;

    std::vector<InstanceGeometryAddresses> instance_addresses_;

    // Helper vector containing pointers to FixDeviceAddress commands
    std::vector<FixDeviceAddress*> fix_device_address_commands_;

    // Updated on each UpdateDescriptorSets command, represents descriptor data
    std::unordered_map<format::HandleId, std::vector<DescriptorData>> descriptor_data_;

    // Populated for each command buffer on CmdBindDescriptorSets
    std::unordered_map<format::HandleId, std::vector<DescriptorBinding>> bound_descriptor_sets_;

    // ----------------pipeline handle-------------------group index--------SGH value
    std::unordered_map<format::HandleId, std::unordered_map<uint32_t, std::vector<uint8_t>>>
        shader_group_handle_entries_;

    // Helper vector with pointers to vkCmdTraceRays commands
    std::vector<CmdTraceRays*> cmd_trace_rays_commands_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_TOOLS_OPTIMIZE_VULKAN_RAYTRACING_OPTIMIZER_H
