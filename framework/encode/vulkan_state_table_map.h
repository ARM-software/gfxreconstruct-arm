#include <mutex>
#include <unordered_map>

template <typename KeyT, typename ValueT>
struct UnorderedStateMap : public std::unordered_map<KeyT, ValueT>
{
    mutable std::recursive_mutex mutex;
};