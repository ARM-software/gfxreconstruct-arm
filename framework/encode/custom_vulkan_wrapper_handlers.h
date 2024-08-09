#include "encode/vulkan_handle_wrappers.h"
#include <map>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(encode)
using namespace vulkan_wrappers;
struct CustomVulkanStateTable
{
    using WrapperHandle = format::HandleId;
    std::map<WrapperHandle, DebugUtilsObjectNameInfoWrapper> object_name_info_map_;

    void InsertWrapper(DebugUtilsObjectNameInfoWrapper wrapper)
    {
        object_name_info_map_[wrapper.wrapper_handle] = wrapper;
    }

    void RemoveWrapper(const DebugUtilsObjectNameInfoWrapper* wrapper)
    {
        object_name_info_map_.erase(wrapper->wrapper_handle);
    }

    DebugUtilsObjectNameInfoWrapper* GetDebugUtilsObjectNameInfoWrapper(WrapperHandle handle)
    {
        auto entry = object_name_info_map_.find(handle);
        return (entry != object_name_info_map_.end()) ? &entry->second : nullptr;
    }

    void VisitWrappers(std::function<void(DebugUtilsObjectNameInfoWrapper*)> visitor) const
    {
        for (auto entry : object_name_info_map_)
        {
            visitor(&entry.second);
        }
    }
};

GFXRECON_END_NAMESPACE(encode)
GFXRECON_END_NAMESPACE(gfxrecon)