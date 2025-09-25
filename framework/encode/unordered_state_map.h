#ifndef GFXRECON_UNORDERED_STATE_MAP_H
#define GFXRECON_UNORDERED_STATE_MAP_H

#include <mutex>
#include <unordered_map>

template <typename KeyT, typename ValueT>
struct UnorderedStateMap : public std::unordered_map<KeyT, ValueT>
{
    mutable std::recursive_mutex mutex;
};

#endif // GFXRECON_UNORDERED_STATE_MAP_H