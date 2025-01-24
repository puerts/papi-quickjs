#include "pesapi.h"
#include "quickjs.h"
#include <EASTL/shared_ptr.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/allocator_malloc.h>

void GetPapiQuickjsImpl()
{
    eastl::basic_string<char, eastl::allocator_malloc> str = "hello world";
}

struct pesapi_env_ref__
{
    explicit pesapi_env_ref__(JSContext *ctx)
        : context_persistent(JS_DupContext(ctx))
        , ref_count(1)
        //, env_life_cycle_tracker(puerts::DataTransfer::GetJsEnvLifeCycleTracker(context->GetIsolate()))
    {
    }
    
    ~pesapi_env_ref__()
    {
        JS_FreeContext(context_persistent);
    }

    JSContext *context_persistent;
    int ref_count;
    eastl::weak_ptr<int> env_life_cycle_tracker;
};

struct pesapi_value__ {
	explicit pesapi_value__(JSValue jsvalue)
		: v(jsvalue)
	{
	}
	JSValue v;
};

struct pesapi_value_ref__ : pesapi_env_ref__
{
    explicit pesapi_value_ref__(JSContext *ctx, JSValue v, uint32_t field_count)
        : pesapi_env_ref__(ctx), value_persistent(JS_DupValue(ctx, v)), internal_field_count(field_count)
    {
    }
    
    ~pesapi_value_ref__()
    {
        JS_FreeValue(context_persistent, value_persistent);
    }

    JSValue value_persistent;
    uint32_t internal_field_count;
    void* internal_fields[0];
};

namespace qjsimpl
{
static struct pesapi_scope__ *getCurrentScope(JSContext *ctx)
{
	return (struct pesapi_scope__ *) JS_GetContextOpaque(ctx);
}

static void setCurrentScope(JSContext *ctx, struct pesapi_scope__ *scope)
{
	JS_SetContextOpaque(ctx, scope);
}
}

struct pesapi_scope__
{
    const static size_t SCOPE_FIX_SIZE_VALUES_SIZE = 4;
    
    explicit pesapi_scope__(JSContext *ctx)
	{
		this->ctx = ctx;
		prev_scope = qjsimpl::getCurrentScope(ctx);
		qjsimpl::setCurrentScope(ctx, this);

		values_used = 0;

		exception = JS_NULL;
	}

	JSContext *ctx;

	pesapi_scope__ *prev_scope;

	JSValue values[SCOPE_FIX_SIZE_VALUES_SIZE];

	size_t values_used;

	eastl::vector<JSValue*, eastl::allocator_malloc> dynamic_alloc_values;

	JSValue exception;

	JSValue *allocValue()
	{
		JSValue *ret;
		if (values_used < SCOPE_FIX_SIZE_VALUES_SIZE)
		{
			ret = &(values[values_used++]);
		}
		else
		{
			ret = (JSValue *) js_malloc(ctx, sizeof(JSValue));
			dynamic_alloc_values.push_back(ret);
		}
		*ret = JS_UNDEFINED;
		return ret;
	}


	~pesapi_scope__()
	{
		JS_FreeValue(ctx, exception);
		for (size_t i = 0; i < values_used; i++)
		{
			JS_FreeValue(ctx, values[i]);
		}

		for (size_t i = 0; i < dynamic_alloc_values.size(); i++)
		{
			JS_FreeValue(ctx, *dynamic_alloc_values[i]);
			js_free(ctx, dynamic_alloc_values[i]);
		}
		dynamic_alloc_values.clear();
		qjsimpl::setCurrentScope(ctx, prev_scope);
	}
};

static_assert(sizeof(pesapi_scope_memory) >= sizeof(pesapi_scope__), "sizeof(pesapi_scope__) > sizeof(pesapi_scope_memory__)");

namespace qjsimpl
{
inline pesapi_value pesapiValueFromQjsValue(JSValue* v)
{
    return reinterpret_cast<pesapi_value>(v);
}

inline JSValue* qjsValueFromPesapiValue(pesapi_value v)
{
    return reinterpret_cast<JSValue*>(v);
}

inline pesapi_env pesapiEnvFromQjsContext(JSContext * ctx)
{
    return reinterpret_cast<pesapi_env>(ctx);
}

inline JSContext* qjsContextFromPesapiEnv(pesapi_env v)
{
    return reinterpret_cast<JSContext*>(v);
}

static JSValue *allocValueInCurrentScope(JSContext *ctx)
{
	auto scope = getCurrentScope(ctx);
	return scope->allocValue();
}

JSValue literal_values_undefined = JS_UNDEFINED;
JSValue literal_values_null = JS_NULL;
JSValue literal_values_true = JS_TRUE;
JSValue literal_values_false = JS_FALSE;

template<typename Func>
pesapi_value pesapi_create_generic0(pesapi_env env, Func createFunc)
{
    auto ctx = qjsContextFromPesapiEnv(env);
    if (ctx)
    {
        auto ret = allocValueInCurrentScope(ctx);
        if (ret)
        {
            *ret = createFunc(ctx);
            return pesapiValueFromQjsValue(ret);
        }
    }
    return nullptr;
}

template<typename T, typename Func>
pesapi_value pesapi_create_generic1(pesapi_env env, T value, Func createFunc)
{
    auto ctx = qjsContextFromPesapiEnv(env);
    if (ctx)
    {
        auto ret = allocValueInCurrentScope(ctx);
        if (ret)
        {
            *ret = createFunc(ctx, value);
            return pesapiValueFromQjsValue(ret);
        }
    }
    return nullptr;
}

template<typename T1, typename T2, typename Func>
pesapi_value pesapi_create_generic2(pesapi_env env, T1 v1, T2 v2, Func createFunc)
{
    auto ctx = qjsContextFromPesapiEnv(env);
    if (ctx)
    {
        auto ret = allocValueInCurrentScope(ctx);
        if (ret)
        {
            *ret = createFunc(ctx, v1, v2);
            return pesapiValueFromQjsValue(ret);
        }
    }
    return nullptr;
}

template<typename T, typename Func>
T pesapi_get_value_generic(pesapi_env env, pesapi_value pvalue, Func convertFunc)
{
    auto ctx = qjsContextFromPesapiEnv(env);
    if (ctx != nullptr)
    {
        T ret = 0;
        convertFunc(ctx, &ret, pvalue->v);
        return ret;
    }
    return 0;
}


// value process
pesapi_value pesapi_create_null(pesapi_env env)
{
    return pesapiValueFromQjsValue(&literal_values_null); //避免在Scope上分配
}

pesapi_value pesapi_create_undefined(pesapi_env env)
{
    return pesapiValueFromQjsValue(&literal_values_undefined);
}

pesapi_value pesapi_create_boolean(pesapi_env env, bool value)
{
    return pesapiValueFromQjsValue(value ? &literal_values_true : &literal_values_false);
}

pesapi_value pesapi_create_int32(pesapi_env env, int32_t value)
{
    return pesapi_create_generic1(env, value, JS_NewInt32);
}

pesapi_value pesapi_create_uint32(pesapi_env env, uint32_t value)
{
    return pesapi_create_generic1(env, value, JS_NewUint32);
}

pesapi_value pesapi_create_int64(pesapi_env env, int64_t value)
{
    return pesapi_create_generic1(env, value, JS_NewBigInt64);
}

pesapi_value pesapi_create_uint64(pesapi_env env, uint64_t value)
{
    return pesapi_create_generic1(env, value, JS_NewBigUint64);
}

pesapi_value pesapi_create_double(pesapi_env env, double value)
{
    return pesapi_create_generic1(env, value, JS_NewFloat64);
}

pesapi_value pesapi_create_string_utf8(pesapi_env env, const char *str, size_t length)
{
    return pesapi_create_generic2(env, str, length, JS_NewStringLen);
}

static JSValue JS_NewArrayBufferWrap(JSContext *ctx, void *bin, size_t len)
{
    return JS_NewArrayBuffer(ctx, (uint8_t *) bin, len, nullptr, nullptr, false);
}

pesapi_value pesapi_create_binary(pesapi_env env, void *bin, size_t length)
{
    return pesapi_create_generic2(env, bin, length, JS_NewArrayBufferWrap);
}

pesapi_value pesapi_create_array(pesapi_env env)
{
    return pesapi_create_generic0(env, JS_NewArray);
}

pesapi_value pesapi_create_object(pesapi_env env)
{
    return pesapi_create_generic0(env, JS_NewObject);
}
/*
pesapi_value pesapi_create_function(pesapi_env env, pesapi_callback native_impl, void* data, pesapi_function_finalize finalize)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto func = puerts::DataTransfer::IsolateData<puerts::ICppObjectMapper>(context->GetIsolate())->CreateFunction(context, native_impl, data, finalize);
    if (func.IsEmpty())
        return nullptr;
    return v8impl::PesapiValueFromV8LocalValue(func.ToLocalChecked());
}

pesapi_value pesapi_create_class(pesapi_env env, const void* type_id)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto cls = puerts::DataTransfer::IsolateData<puerts::ICppObjectMapper>(context->GetIsolate())->LoadTypeById(context, type_id);
    if (cls.IsEmpty())
        return nullptr;
    return v8impl::PesapiValueFromV8LocalValue(cls.ToLocalChecked());
}
*/

bool pesapi_get_value_bool(pesapi_env env, pesapi_value pvalue)
{
    auto ctx = qjsContextFromPesapiEnv(env);
    if (ctx != nullptr)
    {
        return JS_ToBool(ctx, pvalue->v);
    }
    return false;
}

int32_t pesapi_get_value_int32(pesapi_env env, pesapi_value pvalue)
{
    return pesapi_get_value_generic<int32_t>(env, pvalue, JS_ToInt32);
}

uint32_t pesapi_get_value_uint32(pesapi_env env, pesapi_value pvalue)
{
    return pesapi_get_value_generic<uint32_t>(env, pvalue, JS_ToUint32);
}

int64_t pesapi_get_value_int64(pesapi_env env, pesapi_value pvalue)
{
    return pesapi_get_value_generic<int64_t>(env, pvalue, JS_ToBigInt64);
}

uint64_t pesapi_get_value_uint64(pesapi_env env, pesapi_value pvalue)
{
    return pesapi_get_value_generic<uint64_t>(env, pvalue, JS_ToBigUint64);
}

double pesapi_get_value_double(pesapi_env env, pesapi_value pvalue)
{
    return pesapi_get_value_generic<double>(env, pvalue, JS_ToFloat64);
}

/*
const char* pesapi_get_value_string_utf8(pesapi_env env, pesapi_value pvalue, char* buf, size_t* bufsize)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);

    if (buf == nullptr)
    {
        auto str = value->ToString(context).ToLocalChecked();
        *bufsize = str->Utf8Length(context->GetIsolate());
    }
    else
    {
        auto str = value->ToString(context).ToLocalChecked();
        str->WriteUtf8(context->GetIsolate(), buf, *bufsize);
    }
    return buf;
}

void* pesapi_get_value_binary(pesapi_env env, pesapi_value pvalue, size_t* bufsize)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);

    if (value->IsArrayBufferView())
    {
        v8::Local<v8::ArrayBufferView> buffView = value.As<v8::ArrayBufferView>();
        *bufsize = buffView->ByteLength();
        auto Ab = buffView->Buffer();
        return static_cast<char*>(puerts::DataTransfer::GetArrayBufferData(Ab)) + buffView->ByteOffset();
    }
    if (value->IsArrayBuffer())
    {
        auto ab = v8::Local<v8::ArrayBuffer>::Cast(value);
        return puerts::DataTransfer::GetArrayBufferData(ab, *bufsize);
    }
    return nullptr;
}

uint32_t pesapi_get_array_length(pesapi_env env, pesapi_value pvalue)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    if (value->IsArray())
    {
        return value.As<v8::Array>()->Length();
    }
    return 0;
}

bool pesapi_is_null(pesapi_env env, pesapi_value pvalue)
{
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    return value->IsNull();
}

bool pesapi_is_undefined(pesapi_env env, pesapi_value pvalue)
{
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    return value->IsUndefined();
}

bool pesapi_is_boolean(pesapi_env env, pesapi_value pvalue)
{
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    return value->IsBoolean();
}

bool pesapi_is_int32(pesapi_env env, pesapi_value pvalue)
{
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    return value->IsInt32();
}

bool pesapi_is_uint32(pesapi_env env, pesapi_value pvalue)
{
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    return value->IsUint32();
}

bool pesapi_is_int64(pesapi_env env, pesapi_value pvalue)
{
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    return value->IsBigInt();
}

bool pesapi_is_uint64(pesapi_env env, pesapi_value pvalue)
{
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    return value->IsBigInt();
}

bool pesapi_is_double(pesapi_env env, pesapi_value pvalue)
{
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    return value->IsNumber();
}

bool pesapi_is_string(pesapi_env env, pesapi_value pvalue)
{
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    return value->IsString();
}

bool pesapi_is_object(pesapi_env env, pesapi_value pvalue)
{
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    return value->IsObject();
}

bool pesapi_is_function(pesapi_env env, pesapi_value pvalue)
{
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    return value->IsFunction();
}

bool pesapi_is_binary(pesapi_env env, pesapi_value pvalue)
{
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    return value->IsArrayBuffer() || value->IsArrayBufferView();
}

bool pesapi_is_array(pesapi_env env, pesapi_value pvalue)
{
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    return value->IsArray();
}

pesapi_value pesapi_native_object_to_value(pesapi_env env, const void* type_id, void* object_ptr, bool call_finalize)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    return v8impl::PesapiValueFromV8LocalValue(
        ::puerts::DataTransfer::FindOrAddCData(context->GetIsolate(), context, type_id, object_ptr, !call_finalize));
}

void* pesapi_get_native_object_ptr(pesapi_env env, pesapi_value pvalue)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    if (value.IsEmpty() || !value->IsObject())
        return nullptr;
    return puerts::DataTransfer::GetPointerFast<void>(value.As<v8::Object>());
}

const void* pesapi_get_native_object_typeid(pesapi_env env, pesapi_value pvalue)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    if (value.IsEmpty() || !value->IsObject())
        return nullptr;
    return puerts::DataTransfer::GetPointerFast<void>(value.As<v8::Object>(), 1);
}

bool pesapi_is_instance_of(pesapi_env env, const void* type_id, pesapi_value pvalue)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    return ::puerts::DataTransfer::IsInstanceOf(context->GetIsolate(), static_cast<const char*>(type_id), value.As<v8::Object>());
}

pesapi_value pesapi_boxing(pesapi_env env, pesapi_value pvalue)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);

    auto result = v8::Object::New(context->GetIsolate());
    auto _unused = result->Set(context, 0, value);
    return v8impl::PesapiValueFromV8LocalValue(result);
}

pesapi_value pesapi_unboxing(pesapi_env env, pesapi_value pvalue)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);

    auto outer = value->ToObject(context).ToLocalChecked();
    auto realvalue = outer->Get(context, 0).ToLocalChecked();
    return v8impl::PesapiValueFromV8LocalValue(realvalue);
}

void pesapi_update_boxed_value(pesapi_env env, pesapi_value boxed_value, pesapi_value pvalue)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto holder = v8impl::V8LocalValueFromPesapiValue(boxed_value);
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    if (holder->IsObject())
    {
        auto outer = holder->ToObject(context).ToLocalChecked();
        auto _unused = outer->Set(context, 0, value);
    }
}

bool pesapi_is_boxed_value(pesapi_env env, pesapi_value value)
{
    return pesapi_is_object(env, value);
}

int pesapi_get_args_len(pesapi_callback_info pinfo)
{
    auto info = reinterpret_cast<const v8::FunctionCallbackInfo<v8::Value>*>(pinfo);
    return (*info).Length();
}

pesapi_value pesapi_get_arg(pesapi_callback_info pinfo, int index)
{
    auto info = reinterpret_cast<const v8::FunctionCallbackInfo<v8::Value>*>(pinfo);
    return v8impl::PesapiValueFromV8LocalValue((*info)[index]);
}

PESAPI_EXTERN pesapi_env pesapi_get_env(pesapi_callback_info pinfo)
{
    auto info = reinterpret_cast<const v8::FunctionCallbackInfo<v8::Value>*>(pinfo);
    return v8impl::PesapiEnvFromV8LocalContext((*info).GetIsolate()->GetCurrentContext());
}

pesapi_value pesapi_get_this(pesapi_callback_info pinfo)
{
    auto info = reinterpret_cast<const v8::FunctionCallbackInfo<v8::Value>*>(pinfo);
    return v8impl::PesapiValueFromV8LocalValue((*info).This());
}

pesapi_value pesapi_get_holder(pesapi_callback_info pinfo)
{
    auto info = reinterpret_cast<const v8::FunctionCallbackInfo<v8::Value>*>(pinfo);
    return v8impl::PesapiValueFromV8LocalValue((*info).Holder());
}

void* pesapi_get_userdata(pesapi_callback_info pinfo)
{
    auto info = reinterpret_cast<const v8::FunctionCallbackInfo<v8::Value>*>(pinfo);
    return *(static_cast<void**>(v8::Local<v8::External>::Cast((*info).Data())->Value()));
}

void pesapi_add_return(pesapi_callback_info pinfo, pesapi_value value)
{
    auto info = reinterpret_cast<const v8::FunctionCallbackInfo<v8::Value>*>(pinfo);
    (*info).GetReturnValue().Set(v8impl::V8LocalValueFromPesapiValue(value));
}

void pesapi_throw_by_string(pesapi_callback_info pinfo, const char* msg)
{
    auto info = reinterpret_cast<const v8::FunctionCallbackInfo<v8::Value>*>(pinfo);
    v8::Isolate* isolate = info->GetIsolate();
    isolate->ThrowException(
        v8::Exception::Error(v8::String::NewFromUtf8(isolate, msg, v8::NewStringType::kNormal).ToLocalChecked()));
}

pesapi_env_ref pesapi_create_env_ref(pesapi_env env)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    return new pesapi_env_ref__(context);
}

bool pesapi_env_ref_is_valid(pesapi_env_ref env_ref)
{
    return !env_ref->env_life_cycle_tracker.expired();
}

pesapi_env pesapi_get_env_from_ref(pesapi_env_ref env_ref)
{
    if (env_ref->env_life_cycle_tracker.expired())
    {
        return nullptr;
    }
    return v8impl::PesapiEnvFromV8LocalContext(env_ref->context_persistent.Get(env_ref->isolate));
}

pesapi_env_ref pesapi_duplicate_env_ref(pesapi_env_ref env_ref)
{
    ++env_ref->ref_count;
    return env_ref;
}

void pesapi_release_env_ref(pesapi_env_ref env_ref)
{
    if (--env_ref->ref_count == 0)
    {
        if (env_ref->env_life_cycle_tracker.expired())
        {
#if V8_MAJOR_VERSION < 11
            env_ref->context_persistent.Empty();
            delete env_ref;
#else
            ::operator delete(static_cast<void*>(env_ref));
#endif
        }
        else
        {
            delete env_ref;
        }
    }
}

pesapi_scope pesapi_open_scope(pesapi_env_ref env_ref)
{
    if (env_ref->env_life_cycle_tracker.expired())
    {
        return nullptr;
    }
    env_ref->isolate->Enter();
    auto scope = new pesapi_scope__(env_ref->isolate);
    env_ref->context_persistent.Get(env_ref->isolate)->Enter();
    return scope;
}

pesapi_scope pesapi_open_scope_placement(pesapi_env_ref env_ref, struct pesapi_scope_memory* memory)
{
    if (env_ref->env_life_cycle_tracker.expired())
    {
        return nullptr;
    }
    env_ref->isolate->Enter();
    auto scope = new (memory) pesapi_scope__(env_ref->isolate);
    env_ref->context_persistent.Get(env_ref->isolate)->Enter();
    return scope;
}

bool pesapi_has_caught(pesapi_scope scope)
{
    return scope && scope->trycatch.HasCaught();
}

const char* pesapi_get_exception_as_string(pesapi_scope scope, bool with_stack)
{
    if (!scope)
        return nullptr;
    scope->errinfo = *v8::String::Utf8Value(scope->scope.GetIsolate(), scope->trycatch.Exception());
    if (with_stack)
    {
        auto isolate = scope->scope.GetIsolate();
        v8::Local<v8::Context> context(isolate->GetCurrentContext());
        v8::Local<v8::Message> message = scope->trycatch.Message();

        // 输出 (filename):(line number): (message).
        std::ostringstream stm;
        v8::String::Utf8Value fileName(isolate, message->GetScriptResourceName());
        int lineNum = message->GetLineNumber(context).FromJust();
        stm << *fileName << ":" << lineNum << ": " << scope->errinfo.c_str();

        stm << std::endl;

        // 输出调用栈信息
        v8::Local<v8::Value> stackTrace;
        if (scope->trycatch.StackTrace(context).ToLocal(&stackTrace))
        {
            v8::String::Utf8Value stackTraceVal(isolate, stackTrace);
            stm << std::endl << *stackTraceVal;
        }
        scope->errinfo = stm.str().c_str();
    }
    return scope->errinfo.c_str();
}

void pesapi_close_scope(pesapi_scope scope)
{
    if (!scope)
        return;
    auto isolate = scope->scope.GetIsolate();
    isolate->GetCurrentContext()->Exit();
    delete (scope);
    isolate->Exit();
}

void pesapi_close_scope_placement(pesapi_scope scope)
{
    if (!scope)
        return;
    auto isolate = scope->scope.GetIsolate();
    isolate->GetCurrentContext()->Exit();
    scope->~pesapi_scope__();
    isolate->Exit();
}

pesapi_value_ref pesapi_create_value_ref(pesapi_env env, pesapi_value pvalue, uint32_t internal_field_count)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    size_t totalSize = sizeof(pesapi_value_ref__) + sizeof(void*) * internal_field_count;
    void* buffer = ::operator new(totalSize);
    return new (buffer) pesapi_value_ref__(context, value, internal_field_count);
}

pesapi_value_ref pesapi_duplicate_value_ref(pesapi_value_ref value_ref)
{
    ++value_ref->ref_count;
    return value_ref;
}

void pesapi_release_value_ref(pesapi_value_ref value_ref)
{
    if (--value_ref->ref_count == 0)
    {
        if (!value_ref->env_life_cycle_tracker.expired())
        {
            value_ref->~pesapi_value_ref__();
        }
        ::operator delete(static_cast<void*>(value_ref));
    }
}

pesapi_value pesapi_get_value_from_ref(pesapi_env env, pesapi_value_ref value_ref)
{
    return v8impl::PesapiValueFromV8LocalValue(value_ref->value_persistent.Get(value_ref->isolate));
}

void pesapi_set_ref_weak(pesapi_env env, pesapi_value_ref value_ref)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    value_ref->value_persistent.SetWeak();
}

bool pesapi_set_owner(pesapi_env env, pesapi_value pvalue, pesapi_value powner)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);
    auto owner = v8impl::V8LocalValueFromPesapiValue(powner);

    if (owner->IsObject())
    {
        auto jsObj = owner.template As<v8::Object>();
#if V8_MAJOR_VERSION < 8
        jsObj->Set(context, v8::String::NewFromUtf8(context->GetIsolate(), "_p_i_only_one_child").ToLocalChecked(), value).Check();
#else
        jsObj->Set(context, v8::String::NewFromUtf8Literal(context->GetIsolate(), "_p_i_only_one_child"), value).Check();
#endif
        return true;
    }
    return false;
}

pesapi_env_ref pesapi_get_ref_associated_env(pesapi_value_ref value_ref)
{
    return value_ref;
}

void** pesapi_get_ref_internal_fields(pesapi_value_ref value_ref, uint32_t* pinternal_field_count)
{
    *pinternal_field_count = value_ref->internal_field_count;
    return &value_ref->internal_fields[0];
}

pesapi_value pesapi_get_property(pesapi_env env, pesapi_value pobject, const char* key)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto object = v8impl::V8LocalValueFromPesapiValue(pobject);
    if (object->IsObject())
    {
        auto MaybeValue = object.As<v8::Object>()->Get(
            context, v8::String::NewFromUtf8(context->GetIsolate(), key, v8::NewStringType::kNormal).ToLocalChecked());
        v8::Local<v8::Value> Val;
        if (MaybeValue.ToLocal(&Val))
        {
            return v8impl::PesapiValueFromV8LocalValue(Val);
        }
    }
    return pesapi_create_undefined(env);
}

void pesapi_set_property(pesapi_env env, pesapi_value pobject, const char* key, pesapi_value pvalue)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto object = v8impl::V8LocalValueFromPesapiValue(pobject);
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);

    if (object->IsObject())
    {
        auto _un_used = object.As<v8::Object>()->Set(
            context, v8::String::NewFromUtf8(context->GetIsolate(), key, v8::NewStringType::kNormal).ToLocalChecked(), value);
    }
}

bool pesapi_get_private(pesapi_env env, pesapi_value pobject, void** out_ptr)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto object = v8impl::V8LocalValueFromPesapiValue(pobject);
    if (object.IsEmpty() || !object->IsObject())
    {
        *out_ptr = nullptr;
        return false;
    }
    *out_ptr = puerts::DataTransfer::IsolateData<puerts::ICppObjectMapper>(context->GetIsolate())
                   ->GetPrivateData(context, object.As<v8::Object>());
    return true;
}

bool pesapi_set_private(pesapi_env env, pesapi_value pobject, void* ptr)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto object = v8impl::V8LocalValueFromPesapiValue(pobject);
    if (object.IsEmpty() || !object->IsObject())
    {
        return false;
    }
    puerts::DataTransfer::IsolateData<puerts::ICppObjectMapper>(context->GetIsolate())
        ->SetPrivateData(context, object.As<v8::Object>(), ptr);
    return true;
}

pesapi_value pesapi_get_property_uint32(pesapi_env env, pesapi_value pobject, uint32_t key)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto object = v8impl::V8LocalValueFromPesapiValue(pobject);
    if (object->IsObject())
    {
        auto MaybeValue = object.As<v8::Object>()->Get(context, key);
        v8::Local<v8::Value> Val;
        if (MaybeValue.ToLocal(&Val))
        {
            return v8impl::PesapiValueFromV8LocalValue(Val);
        }
    }
    return pesapi_create_undefined(env);
}

void pesapi_set_property_uint32(pesapi_env env, pesapi_value pobject, uint32_t key, pesapi_value pvalue)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto object = v8impl::V8LocalValueFromPesapiValue(pobject);
    auto value = v8impl::V8LocalValueFromPesapiValue(pvalue);

    if (object->IsObject())
    {
        auto _un_used = object.As<v8::Object>()->Set(context, key, value);
    }
}

pesapi_value pesapi_call_function(pesapi_env env, pesapi_value pfunc, pesapi_value this_object, int argc, const pesapi_value argv[])
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    v8::Local<v8::Value> recv = v8::Undefined(context->GetIsolate());
    if (this_object)
    {
        recv = v8impl::V8LocalValueFromPesapiValue(this_object);
    }
    v8::Local<v8::Function> func = v8impl::V8LocalValueFromPesapiValue(pfunc).As<v8::Function>();

    auto maybe_ret = func->Call(context, recv, argc, reinterpret_cast<v8::Local<v8::Value>*>(const_cast<pesapi_value*>(argv)));
    if (maybe_ret.IsEmpty())
    {
        return nullptr;
    }
    return v8impl::PesapiValueFromV8LocalValue(maybe_ret.ToLocalChecked());
}

pesapi_value pesapi_eval(pesapi_env env, const uint8_t* code, size_t code_size, const char* path)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto isolate = context->GetIsolate();
    v8::Local<v8::String> url =
        v8::String::NewFromUtf8(isolate, path == nullptr ? "" : path, v8::NewStringType::kNormal).ToLocalChecked();
    std::vector<char> buff;
    buff.reserve(code_size + 1);
    memcpy(buff.data(), code, code_size);
    buff.data()[code_size] = '\0';
    v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, buff.data(), v8::NewStringType::kNormal).ToLocalChecked();
#if V8_MAJOR_VERSION > 8
    v8::ScriptOrigin origin(isolate, url);
#else
    v8::ScriptOrigin origin(url);
#endif

    auto CompiledScript = v8::Script::Compile(context, source, &origin);
    if (CompiledScript.IsEmpty())
    {
        return nullptr;
    }
    auto maybe_ret = CompiledScript.ToLocalChecked()->Run(context);
    if (maybe_ret.IsEmpty())
    {
        return nullptr;
    }
    return v8impl::PesapiValueFromV8LocalValue(maybe_ret.ToLocalChecked());
}

pesapi_value pesapi_global(pesapi_env env)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    auto global = context->Global();
    return v8impl::PesapiValueFromV8LocalValue(global);
}

const void* pesapi_get_env_private(pesapi_env env)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    return puerts::DataTransfer::GetIsolatePrivateData(context->GetIsolate());
}

void pesapi_set_env_private(pesapi_env env, const void* ptr)
{
    auto context = v8impl::V8LocalContextFromPesapiEnv(env);
    puerts::DataTransfer::SetIsolatePrivateData(context->GetIsolate(), const_cast<void*>(ptr));
}

pesapi_ffi g_pesapi_ffi {
    &pesapi_create_null,
    &pesapi_create_undefined,
    &pesapi_create_boolean,
    &pesapi_create_int32,
    &pesapi_create_uint32,
    &pesapi_create_int64,
    &pesapi_create_uint64,
    &pesapi_create_double,
    &pesapi_create_string_utf8,
    &pesapi_create_binary,
    &pesapi_create_array,
    &pesapi_create_object,
    &pesapi_create_function,
    &pesapi_create_class,
    &pesapi_get_value_bool,
    &pesapi_get_value_int32,
    &pesapi_get_value_uint32,
    &pesapi_get_value_int64,
    &pesapi_get_value_uint64,
    &pesapi_get_value_double,
    &pesapi_get_value_string_utf8,
    &pesapi_get_value_binary,
    &pesapi_get_array_length,
    &pesapi_is_null,
    &pesapi_is_undefined,
    &pesapi_is_boolean,
    &pesapi_is_int32,
    &pesapi_is_uint32,
    &pesapi_is_int64,
    &pesapi_is_uint64,
    &pesapi_is_double,
    &pesapi_is_string,
    &pesapi_is_object,
    &pesapi_is_function,
    &pesapi_is_binary,
    &pesapi_is_array,
    &pesapi_native_object_to_value,
    &pesapi_get_native_object_ptr,
    &pesapi_get_native_object_typeid,
    &pesapi_is_instance_of,
    &pesapi_boxing,
    &pesapi_unboxing,
    &pesapi_update_boxed_value,
    &pesapi_is_boxed_value,
    &pesapi_get_args_len,
    &pesapi_get_arg,
    &pesapi_get_env,
    &pesapi_get_this,
    &pesapi_get_holder,
    &pesapi_get_userdata,
    &pesapi_add_return,
    &pesapi_throw_by_string,
    &pesapi_create_env_ref,
    &pesapi_env_ref_is_valid,
    &pesapi_get_env_from_ref,
    &pesapi_duplicate_env_ref,
    &pesapi_release_env_ref,
    &pesapi_open_scope,
    &pesapi_open_scope_placement,
    &pesapi_has_caught,
    &pesapi_get_exception_as_string,
    &pesapi_close_scope,
    &pesapi_close_scope_placement,
    &pesapi_create_value_ref,
    &pesapi_duplicate_value_ref,
    &pesapi_release_value_ref,
    &pesapi_get_value_from_ref,
    &pesapi_set_ref_weak,
    &pesapi_set_owner,
    &pesapi_get_ref_associated_env,
    &pesapi_get_ref_internal_fields,
    &pesapi_get_property,
    &pesapi_set_property,
    &pesapi_get_private,
    &pesapi_set_private,
    &pesapi_get_property_uint32,
    &pesapi_set_property_uint32,
    &pesapi_call_function,
    &pesapi_eval,
    &pesapi_global,
    &pesapi_get_env_private,
    &pesapi_set_env_private
};
*/
}    // namespace qjsimpl
