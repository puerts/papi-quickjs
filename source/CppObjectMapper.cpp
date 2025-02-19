#include "CppObjectMapper.h"
#include "pesapi.h"

namespace pesapi
{
namespace qjsimpl
{
extern pesapi_ffi g_pesapi_ffi;

void CppObjectMapper::Initialize(JSContext* ctx_)
{
    ctx = ctx_;
    JS_SetContextOpaque(ctx, this);
    new (&CDataCache) eastl::unordered_map<const void*, FObjectCacheNode, eastl::hash<const void*>, 
            eastl::equal_to<const void*>, eastl::allocator_malloc>();
    new (&TypeIdToFunctionMap) eastl::unordered_map<const void*, JSValue, eastl::hash<const void*>, 
            eastl::equal_to<const void*>, eastl::allocator_malloc>();
}

void CppObjectMapper::Cleanup()
{
    if (ctx)
    {
        JS_SetContextOpaque(ctx, nullptr);
    }
    CDataCache.~hash_map();
    TypeIdToFunctionMap.~hash_map();
}

}
}

pesapi_env_ref create_qjs_env()
{
    JSRuntime* rt = JS_NewRuntime();
    JSContext* ctx = JS_NewContext(rt);
    //pesapi::qjsimpl::CppObjectMapper* mapper = new pesapi::qjsimpl::CppObjectMapper();
    //mapper->Initialize(ctx);
    //return pesapi::qjsimpl::g_pesapi_ffi.create_env_ref(reinterpret_cast<pesapi_env>(ctx));
    return nullptr;
}
