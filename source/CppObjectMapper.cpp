#include "CppObjectMapper.h"
#include "pesapi.h"
#include "PapiData.h"

#define JS_TAG_EXTERNAL (JS_TAG_FLOAT64 + 1)

namespace pesapi
{
namespace qjsimpl
{
extern pesapi_ffi g_pesapi_ffi;

void PApiObjectFinalizer(JSRuntime* rt, JSValue val)
{
    CppObjectMapper* mapper = reinterpret_cast<CppObjectMapper*>(JS_GetRuntimeOpaque(rt));
    JS_GetOpaque(val, mapper->classId);
}

struct FuncFinalizeData
{
    pesapi_function_finalize finalize;
    void* data;
    CppObjectMapper* mapper;
};

void PApiFuncFinalizer(JSRuntime* rt, JSValue val)
{
    CppObjectMapper* mapper = reinterpret_cast<CppObjectMapper*>(JS_GetRuntimeOpaque(rt));
    FuncFinalizeData* data = (FuncFinalizeData*)JS_GetOpaque(val, mapper->funcTracerClassId);
    if (data->finalize)
    {
        data->finalize(&g_pesapi_ffi, data->data, (void*)(data->mapper->GetEnvPrivate())); // TODO: env_private 和 get_env_private 的const修饰统一
    }
    js_free_rt(rt, data);
}

JSValue CppObjectMapper::CreateFunction(pesapi_callback Callback, void* Data, pesapi_function_finalize Finalize)
{
    FuncFinalizeData* data = (FuncFinalizeData*)js_malloc(ctx, sizeof(FuncFinalizeData));
    data->finalize = Finalize;
    data->data = Data;
    data->mapper = this;

    JSValue traceObj = JS_NewObjectClass(ctx, funcTracerClassId);
    JS_SetOpaque(traceObj, data);
    JSValue func_data[3] {
        JS_MKPTR(JS_TAG_EXTERNAL, (void*)Callback), 
        JS_MKPTR(JS_TAG_EXTERNAL, Data), 
        traceObj
        };

    JSValue func = JS_NewCFunctionData(ctx, [](JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *func_data) -> JSValue {
        pesapi_callback callback = (pesapi_callback)(JS_VALUE_GET_PTR(func_data[0]));
        pesapi_callback_info__ callbackInfo  { ctx, this_val, argc, argv, JS_VALUE_GET_PTR(func_data[1]), JS_UNDEFINED, JS_UNDEFINED };

        callback(&g_pesapi_ffi, &callbackInfo);
        if (JS_IsException(callbackInfo.res))
        {
            return JS_Throw(ctx, callbackInfo.ex);
        }
        else
        {
            return callbackInfo.res;
        }
    }, 0, 0, 3, &func_data[0]);

    JS_FreeValue(ctx, traceObj); // 在JS_NewCFunctionData有个dup的过程

    return func;
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

    classId = 0;
    JS_NewClassID(rt, &classId);
    JS_NewClass(rt, classId, &cls_def);


    JSClassDef func_tracer_cls_def;
    func_tracer_cls_def.class_name = "__papi_func_tracer";
    func_tracer_cls_def.finalizer = PApiFuncFinalizer;
    func_tracer_cls_def.exotic = NULL;
    func_tracer_cls_def.gc_mark = NULL;
    func_tracer_cls_def.call = NULL;

    funcTracerClassId = 0;
    JS_NewClassID(rt, &funcTracerClassId);
    JS_NewClass(rt, funcTracerClassId, &func_tracer_cls_def);
}

void CppObjectMapper::Cleanup()
{
    CDataCache.clear();
    TypeIdToFunctionMap.clear();
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
    //auto scope = pesapi::qjsimpl::g_pesapi_ffi.open_scope(env_ref);
    JSContext* ctx = reinterpret_cast<JSContext*>(pesapi::qjsimpl::g_pesapi_ffi.get_env_from_ref(env_ref));
    JSRuntime* rt = JS_GetRuntime(ctx);
    pesapi::qjsimpl::CppObjectMapper* mapper = reinterpret_cast<pesapi::qjsimpl::CppObjectMapper*>(JS_GetRuntimeOpaque(rt));
    //pesapi::qjsimpl::g_pesapi_ffi.close_scope(scope);
    pesapi::qjsimpl::g_pesapi_ffi.release_env_ref(env_ref);
    if (mapper)
    {
        mapper->Cleanup();
        mapper->~CppObjectMapper();
        free(mapper);
    }
    
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

struct pesapi_ffi* get_papi_ffi()
{
    return &pesapi::qjsimpl::g_pesapi_ffi;
}

// ----------------end test interface----------------