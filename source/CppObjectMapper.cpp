#include "CppObjectMapper.h"
#include "pesapi.h"

namespace pesapi
{
namespace qjsimpl
{
extern pesapi_ffi g_pesapi_ffi;

void PApiObjectFinalizer(JSRuntime* rt, JSValue val)
{
    CppObjectMapper* mapper = reinterpret_cast<CppObjectMapper*>(JS_GetRuntimeOpaque(rt));
    JS_GetOpaque(val, mapper->class_id);
}

void CppObjectMapper::Initialize(JSContext* ctx_)
{
    ctx = ctx_;
    rt = JS_GetRuntime(ctx);
    JS_SetRuntimeOpaque(rt, this);
    //new (&CDataCache) eastl::unordered_map<const void*, FObjectCacheNode, eastl::hash<const void*>, 
    //        eastl::equal_to<const void*>, eastl::allocator_malloc>();
    //new (&TypeIdToFunctionMap) eastl::unordered_map<const void*, JSValue, eastl::hash<const void*>, 
    //        eastl::equal_to<const void*>, eastl::allocator_malloc>();

    JSClassDef cls_def;
    cls_def.class_name = "__papi_obj";
    cls_def.finalizer = PApiObjectFinalizer;
    cls_def.exotic = NULL;
    cls_def.gc_mark = NULL;
    cls_def.call = NULL;

    class_id = 0;
    JS_NewClassID(rt, &class_id);
    JS_NewClass(rt, class_id, &cls_def);
}

void CppObjectMapper::Cleanup()
{
    if (rt)
    {
        JS_SetRuntimeOpaque(rt, nullptr);
    }
    //CDataCache.~hash_map();
    //TypeIdToFunctionMap.~hash_map();
}

}
}

// ----------------begin test interface----------------

pesapi_env_ref create_qjs_env()
{
    JSRuntime* rt = JS_NewRuntime();
    JSContext* ctx = JS_NewContext(rt);
    pesapi::qjsimpl::CppObjectMapper* mapper = static_cast<pesapi::qjsimpl::CppObjectMapper*>(malloc(sizeof(pesapi::qjsimpl::CppObjectMapper)));
    
    if (mapper)
    {
        memset(mapper, 0, sizeof(pesapi::qjsimpl::CppObjectMapper));
        new (mapper) pesapi::qjsimpl::CppObjectMapper();
        mapper->Initialize(ctx);
        return pesapi::qjsimpl::g_pesapi_ffi.create_env_ref(reinterpret_cast<pesapi_env>(ctx));
    }
    return nullptr;
}

void destroy_qjs_env(pesapi_env_ref env_ref)
{
    auto scope = pesapi::qjsimpl::g_pesapi_ffi.open_scope(env_ref);
    JSContext* ctx = reinterpret_cast<JSContext*>(pesapi::qjsimpl::g_pesapi_ffi.get_env_from_ref(env_ref));
    JSRuntime* rt = JS_GetRuntime(ctx);
    pesapi::qjsimpl::CppObjectMapper* mapper = reinterpret_cast<pesapi::qjsimpl::CppObjectMapper*>(JS_GetRuntimeOpaque(rt));
    pesapi::qjsimpl::g_pesapi_ffi.close_scope(scope);
    if (mapper)
    {
        mapper->Cleanup();
        mapper->~CppObjectMapper();
        free(mapper);
    }
}

struct pesapi_ffi* get_papi_ffi()
{
    return &pesapi::qjsimpl::g_pesapi_ffi;
}

// ----------------end test interface----------------