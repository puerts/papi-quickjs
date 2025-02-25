#include "CppObjectMapper.h"
#include "pesapi.h"
#include "PapiData.h"

#define JS_TAG_EXTERNAL (JS_TAG_FLOAT64 + 1)

namespace pesapi
{
namespace qjsimpl
{
extern pesapi_ffi g_pesapi_ffi;

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

    JS_FreeValue(ctx, traceObj); // 在JS_NewCFunctionData有个duplicate操作，所以这里要free

    return func;
}

JSValue CppObjectMapper::CreateError(JSContext* ctx, const char* message)
{
    JSValue ret = JS_NewError(ctx);
    JS_DefinePropertyValue(ctx, ret, JS_ATOM_message, JS_NewString(ctx, message), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    return ret;
}

void PApiObjectFinalizer(JSRuntime* rt, JSValue val)
{
    CppObjectMapper* mapper = reinterpret_cast<CppObjectMapper*>(JS_GetRuntimeOpaque(rt));
    ObjectUserData* object_udata = (ObjectUserData*)JS_GetOpaque(val, mapper->classId);

    if (object_udata->callFinalize && object_udata->typeInfo->Finalize)
    {
        object_udata->typeInfo->Finalize(&g_pesapi_ffi, (void*)object_udata->ptr, object_udata->typeInfo->Data, (void*)(mapper->GetEnvPrivate()));
    }
    mapper->RemoveFromCache(object_udata->typeInfo, object_udata->ptr);
    js_free_rt(rt, object_udata);
}

void CppObjectMapper::AddToCache(const puerts::JSClassDefinition* typeInfo, const void* ptr, JSValue value, bool callFinalize)
{
    ObjectUserData* object_udata = (ObjectUserData*)js_malloc(ctx, sizeof(ObjectUserData));
    object_udata->typeInfo = typeInfo;
    object_udata->ptr = ptr;
    object_udata->callFinalize = callFinalize;
    
    JS_SetOpaque(value, object_udata);

    auto Iter = CDataCache.find(ptr);
    FObjectCacheNode* CacheNodePtr;
    if (Iter != CDataCache.end())
    {
        CacheNodePtr = Iter->second.Add(typeInfo->TypeId);
    }
    else
    {
        auto Ret = CDataCache.insert({ptr, FObjectCacheNode(typeInfo->TypeId)});
        CacheNodePtr = &Ret.first->second;
    }
    CacheNodePtr->Value = value;
}

void CppObjectMapper::RemoveFromCache(const puerts::JSClassDefinition* typeInfo, const void* ptr)
{
    auto Iter = CDataCache.find(ptr);
    if (Iter != CDataCache.end())
    {
        auto Removed = Iter->second.Remove(typeInfo->TypeId, true);
        if (!Iter->second.TypeId)    // last one
        {
            CDataCache.erase(ptr);
        }
    }
}

void CppObjectMapper::CreateMethod(puerts::JSFunctionInfo* FunctionInfo, JSValue Obj)
{
    JSValue method_data[2] {
            JS_MKPTR(JS_TAG_EXTERNAL, (void*)FunctionInfo),
            JS_MKPTR(JS_TAG_EXTERNAL, this)
            };

    JSValue func = JS_NewCFunctionData(ctx, [](JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *method_data) -> JSValue {
        const puerts::JSFunctionInfo* funcInfo = (const puerts::JSFunctionInfo*)(JS_VALUE_GET_PTR(method_data[0]));
        CppObjectMapper* mapper = (CppObjectMapper*)(JS_VALUE_GET_PTR(method_data[1]));
        
        pesapi_callback_info__ callbackInfo  { ctx, this_val, argc, argv, JS_VALUE_GET_PTR(method_data[1]), JS_UNDEFINED, JS_UNDEFINED };
        funcInfo->Callback(&g_pesapi_ffi, &callbackInfo);
        if (JS_IsException(callbackInfo.res))
        {
            JS_FreeValue(ctx, callbackInfo.res);
            return JS_Throw(ctx, callbackInfo.ex);
        }
        else
        {
            return callbackInfo.res;
        }
    }, 0, 0, 2, &method_data[0]);

    JSAtom methodName = JS_NewAtom(ctx, FunctionInfo->Name);
    JS_DefinePropertyValue(ctx, Obj, methodName, func, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE | JS_PROP_WRITABLE);
    JS_FreeAtom(ctx, methodName);
}

JSValue CppObjectMapper::CreateClass(const puerts::JSClassDefinition* ClassDefinition)
{
    auto it = TypeIdToFunctionMap.find(ClassDefinition->TypeId);
    if (it == TypeIdToFunctionMap.end())
    {
        JSValue ctor_data[2] {
            JS_MKPTR(JS_TAG_EXTERNAL, (void*)ClassDefinition),
            JS_MKPTR(JS_TAG_EXTERNAL, this)
            };

        JSValue func = JS_NewCFunctionData(ctx, [](JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValue *ctor_data) -> JSValue {
            const puerts::JSClassDefinition* clsDef = (const puerts::JSClassDefinition*)(JS_VALUE_GET_PTR(ctor_data[0]));
            CppObjectMapper* mapper = (CppObjectMapper*)(JS_VALUE_GET_PTR(ctor_data[1]));
            
            if (clsDef->Initialize)
            {
                pesapi_callback_info__ callbackInfo  { ctx, this_val, argc, argv, JS_VALUE_GET_PTR(ctor_data[1]), JS_UNDEFINED, JS_UNDEFINED };
                JSValue proto = JS_GetProperty(ctx, this_val, JS_ATOM_prototype);
                callbackInfo.this_val = JS_NewObjectProtoClass(ctx, proto, mapper->classId);
                JS_FreeValue(ctx, proto);
                void* ptr = clsDef->Initialize(&g_pesapi_ffi, &callbackInfo);
                mapper->AddToCache(clsDef, ptr, callbackInfo.this_val, true);
                if (JS_IsException(callbackInfo.res))
                {
                    JS_FreeValue(ctx, callbackInfo.this_val);
                    return JS_Throw(ctx, callbackInfo.ex);
                }
                else
                {
                    return callbackInfo.this_val;
                }
            }
            else
            {
                return JS_Throw(ctx, CppObjectMapper::CreateError(ctx, "no initialize function"));
            }
        }, 0, 0, 2, &ctor_data[0]);

        JS_SetConstructorBit(ctx, func, 1);

        auto clsName = JS_NewAtom(ctx, ClassDefinition->ScriptName);
        JS_DefinePropertyValue( 
            ctx, 
            func, 
            JS_ATOM_name,
            JS_AtomToString(ctx, clsName), 
            JS_PROP_CONFIGURABLE
        );
        JS_FreeAtom(ctx, clsName);

        JSValue proto = JS_NewObject(ctx);

        puerts::JSPropertyInfo* PropertyInfo = ClassDefinition->Properties;
        while (PropertyInfo && PropertyInfo->Name)
        {
            ++PropertyInfo;
        }

        PropertyInfo = ClassDefinition->Variables;
        while (PropertyInfo && PropertyInfo->Name)
        {
            ++PropertyInfo;
        }

        puerts::JSFunctionInfo* FunctionInfo = ClassDefinition->Methods;
        while (FunctionInfo && FunctionInfo->Name && FunctionInfo->Callback)
        {
            CreateMethod(FunctionInfo, proto);
            ++FunctionInfo;
        }

        FunctionInfo = ClassDefinition->Functions;
        while (FunctionInfo && FunctionInfo->Name && FunctionInfo->Callback)
        {
            CreateMethod(FunctionInfo, func);
            ++FunctionInfo;
        }

        JS_SetConstructor(ctx, func, proto);
        JS_FreeValue(ctx, proto);

        TypeIdToFunctionMap[ClassDefinition->TypeId] = func;
        JS_DupValue(ctx, func); //JS_FreeValue in Cleanup
        return func;
    }
    return it->second;
}

JSValue CppObjectMapper::CreateClassByID(const void* typeId)
{
    auto clsDef = puerts::LoadClassByID(typeId);
    if (!clsDef)
    {
        return JS_UNDEFINED;
    }
    return CreateClass(clsDef);
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
    for(auto& kv : TypeIdToFunctionMap)
    {
        JS_FreeValue(ctx, kv.second);
    }
    CDataCache.clear();
    TypeIdToFunctionMap.clear();
    //CDataCache.~hash_map();
    //TypeIdToFunctionMap.~hash_map();
}

}
}

// ----------------begin test interface----------------

pesapi_env_ref create_qjs_env()
{
    JSRuntime* rt = JS_NewRuntime();
    // 0x4000: DUMP_LEAKS, 0x8000: DUMP_ATOM_LEAKS
    JS_SetDumpFlags(rt, 0x4000 | 0x8000);
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
    mapper->Cleanup();
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    if (mapper)
    {
        mapper->~CppObjectMapper();
        free(mapper);
    }
}

struct pesapi_ffi* get_papi_ffi()
{
    return &pesapi::qjsimpl::g_pesapi_ffi;
}

// ----------------end test interface----------------