#pragma once

#include "pesapi.h"
#include "quickjs.h"
#include <EASTL/unordered_map.h>
#include <EASTL/allocator_malloc.h>
#include <EASTL/shared_ptr.h>
#include "ObjectCacheNode.h"

namespace pesapi
{
namespace qjsimpl
{

struct CppObjectMapper
{
    inline static CppObjectMapper* Get(JSContext* ctx)
    {
        return reinterpret_cast<CppObjectMapper*>(JS_GetRuntimeOpaque(JS_GetRuntime(ctx)));
    }

    void Initialize(JSContext* ctx_);

    void Cleanup();
    
    CppObjectMapper(const CppObjectMapper&) = delete;
    CppObjectMapper& operator=(const CppObjectMapper&) = delete;
    
    //void* operator new(size_t) = delete;
    void operator delete(void*) = delete;

    CppObjectMapper() = default;
    ~CppObjectMapper() = default;

    eastl::unordered_map<const void*, FObjectCacheNode, eastl::hash<const void*>, eastl::equal_to<const void*>, eastl::allocator_malloc> CDataCache;
    eastl::unordered_map<const void*, JSValue, eastl::hash<const void*>, eastl::equal_to<const void*>, eastl::allocator_malloc> TypeIdToFunctionMap;

    JSRuntime* rt;
    JSContext* ctx;
    JSClassID classId;
    JSClassID funcTracerClassId;
    const void* envPrivate = nullptr;
    eastl::shared_ptr<int> ref = eastl::allocate_shared<int>(eastl::allocator_malloc("shared_ptr"), 0);

    static eastl::weak_ptr<int> GetEnvLifeCycleTracker(JSContext* ctx)
    {
        JSRuntime* rt = JS_GetRuntime(ctx);
        CppObjectMapper* mapper = reinterpret_cast<CppObjectMapper*>(JS_GetRuntimeOpaque(rt));
        return mapper->GetEnvLifeCycleTracker();
    }

    eastl::weak_ptr<int> GetEnvLifeCycleTracker()
    {
        return eastl::weak_ptr<int>(ref);
    }

    const void* GetEnvPrivate() const
    {
        return envPrivate;
    }

    void SetEnvPrivate(const void* envPrivate_)
    {
        envPrivate = envPrivate_;
    }

    JSValue CreateFunction(pesapi_callback Callback, void* Data, pesapi_function_finalize Finalize);
};

}  // namespace qjsimpl
}  // namespace pesapi

// ----------------begin test interface----------------
PESAPI_MODULE_EXPORT pesapi_env_ref create_qjs_env();
PESAPI_MODULE_EXPORT void destroy_qjs_env(pesapi_env_ref env_ref);
PESAPI_MODULE_EXPORT struct pesapi_ffi* get_papi_ffi();
// ----------------end test interface----------------
