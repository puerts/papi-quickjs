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
