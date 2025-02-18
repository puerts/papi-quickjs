#pragma once

#include "pesapi.h"
#include "quickjs.h"
#include <EASTL/unordered_map.h>
#include <EASTL/allocator_malloc.h>
#include "ObjectCacheNode.h"

namespace pesapi
{
namespace qjsimpl
{

class CppObjectMapper
{
public:
    void Initialize(JSContext* ctx);
private:
    eastl::unordered_map<const void*, FObjectCacheNode, eastl::hash<const void*>, eastl::equal_to<const void*>, eastl::allocator_malloc> CDataCache;
    eastl::unordered_map<const void*, JSValue, eastl::hash<const void*>, eastl::equal_to<const void*>, eastl::allocator_malloc> TypeIdToFunctionMap;
};

}  // namespace qjsimpl
}  // namespace pesapi


PESAPI_MODULE_EXPORT pesapi_env_ref create_qjs_env();