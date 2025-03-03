#pragma once

#include "pesapi.h"
#include "quickjs.h"
#include <EASTL/shared_ptr.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/allocator_malloc.h>
#include "CppObjectMapper.h"

enum
{
	JS_ATOM_NULL_,
#define DEF(name, str) JS_ATOM_##name,
#include "quickjs-atom.h"
#undef DEF
	JS_ATOM_END,
};

struct pesapi_env_ref__
{
    explicit pesapi_env_ref__(JSContext *ctx)
        : context_persistent(JS_DupContext(ctx))
        , ref_count(1)
        , env_life_cycle_tracker(pesapi::qjsimpl::CppObjectMapper::GetEnvLifeCycleTracker(ctx))
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

namespace pesapi
{
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

struct caught_exception_info
{
    JSValue exception = JS_UNDEFINED;
    eastl::basic_string<char, eastl::allocator_malloc> message;
};

}
}

struct pesapi_scope__
{
    const static size_t SCOPE_FIX_SIZE_VALUES_SIZE = 4;
    
    explicit pesapi_scope__(JSContext *ctx)
	{
		this->ctx = ctx;
		prev_scope = pesapi::qjsimpl::getCurrentScope(ctx);
		pesapi::qjsimpl::setCurrentScope(ctx, this);
		values_used = 0;
		caught = nullptr;
	}

	JSContext *ctx;

	pesapi_scope__ *prev_scope;

	JSValue values[SCOPE_FIX_SIZE_VALUES_SIZE];

	uint32_t values_used;

	eastl::vector<JSValue*, eastl::allocator_malloc> dynamic_alloc_values;

	pesapi::qjsimpl::caught_exception_info* caught;

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

    void setCaughtException(JSValue exception)
    {
        if (caught == nullptr)
        {
            caught = (pesapi::qjsimpl::caught_exception_info *) js_malloc(ctx, sizeof(pesapi::qjsimpl::caught_exception_info));
            memset(caught, 0, sizeof(pesapi::qjsimpl::caught_exception_info));
            new (caught) pesapi::qjsimpl::caught_exception_info();
        }
        caught->exception = exception;
    }


	~pesapi_scope__()
	{
        if (caught)
        {
		    JS_FreeValue(ctx, caught->exception);
            caught->~caught_exception_info();
            js_free(ctx, caught);
        }
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
		pesapi::qjsimpl::setCurrentScope(ctx, prev_scope);
	}
};

static_assert(sizeof(pesapi_scope_memory) >= sizeof(pesapi_scope__), "sizeof(pesapi_scope__) > sizeof(pesapi_scope_memory__)");

struct pesapi_callback_info__ {
	JSContext *ctx;
	JSValueConst this_val;
	int argc;
	JSValueConst *argv;
    void* data;
	JSValue res;
	JSValue ex;
};
