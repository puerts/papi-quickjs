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

struct CppObjectMapper
{
    void Initialize(JSContext* ctx_);

    void Cleanup();
    
    CppObjectMapper(const CppObjectMapper&) = delete;
    CppObjectMapper& operator=(const CppObjectMapper&) = delete;
    
    void* operator new(size_t) = delete;
    void operator delete(void*) = delete;

    CppObjectMapper() = default;
    ~CppObjectMapper() = default;

    eastl::unordered_map<const void*, FObjectCacheNode, eastl::hash<const void*>, eastl::equal_to<const void*>, eastl::allocator_malloc> CDataCache;
    eastl::unordered_map<const void*, JSValue, eastl::hash<const void*>, eastl::equal_to<const void*>, eastl::allocator_malloc> TypeIdToFunctionMap;

    JSContext* ctx;
};

}  // namespace qjsimpl
}  // namespace pesapi


PESAPI_MODULE_EXPORT pesapi_env_ref create_qjs_env();