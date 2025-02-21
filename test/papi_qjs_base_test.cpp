#include <gtest/gtest.h>
#include "CppObjectMapper.h"
#include <quickjs/quickjs.h>
#include "pesapi.h"
#include "JSClassRegister.h"

namespace pesapi {
namespace qjsimpl {

class PApiBaseTest : public ::testing::Test {
public:
    static void Foo(struct pesapi_ffi* apis, pesapi_callback_info info)
    {

    }

    static void Bar(struct pesapi_ffi* apis, pesapi_callback_info info)
    {
        auto env = apis->get_env(info);
        PApiBaseTest* self = (PApiBaseTest*)apis->get_userdata(info);
        auto arg0 = apis->get_arg(info, 0);
        if (apis->is_int32(env, arg0))
        {
            self->bar_data = apis->get_value_int32(env, arg0);
        }
    }

    int bar_data = 0;

protected:
    void SetUp() override {
        env_ref = create_qjs_env();
        api = get_papi_ffi();
    }

    void TearDown() override {
        if (env_ref) {
            destroy_qjs_env(env_ref);
        }
    }

    pesapi_env_ref env_ref;
    struct pesapi_ffi* api;
};

TEST_F(PApiBaseTest, CreateAndDestroyMultQjsEnv) {
    //多次调用create_qjs_env和destroy_qjs_env
    for (int i = 0; i < 10; i++) {
        pesapi_env_ref env_ref = create_qjs_env();
        destroy_qjs_env(env_ref);
    }
}

TEST_F(PApiBaseTest, RegApi) {
    pesapi_property_descriptor properties = pesapi_alloc_property_descriptors(1);
    pesapi_set_method_info(properties, 0, "Foo", true, Foo, NULL, NULL);
    pesapi_define_class("Test", NULL, "Test", NULL, NULL, 1, properties, NULL);

    auto clsDef = puerts::FindClassByID("Test");
    ASSERT_TRUE(clsDef != nullptr);

    ASSERT_TRUE(strcmp(clsDef->Functions[0].Name, "Foo") == 0);

    ASSERT_TRUE(clsDef->Functions[0].Callback == Foo);
}

TEST_F(PApiBaseTest, EvalJavaScript) {
    auto scope = api->open_scope(env_ref);
    auto env = api->get_env_from_ref(env_ref);

    auto code = "123+789";
    auto ret = api->eval(env, (const uint8_t*)(code), strlen(code), "test.js");
    ASSERT_TRUE(ret != nullptr);
    ASSERT_TRUE(api->is_int32(env, ret));
    ASSERT_TRUE(api->get_value_int32(env, ret) == 912);

    api->close_scope(scope);
}

TEST_F(PApiBaseTest, EvalJavaScriptEx) {
    auto scope = api->open_scope(env_ref);
    auto env = api->get_env_from_ref(env_ref);

    auto code = " (function() { throw new Error('abc'); }) ();";
    auto ret = api->eval(env, (const uint8_t*)(code), strlen(code), "test.js");
    ASSERT_TRUE(api->has_caught(scope));

    EXPECT_STREQ("Error: abc", api->get_exception_as_string(scope, false));
    EXPECT_STREQ("Error: abc\n    at <anonymous> (test.js:1:21)\n    at <eval> (test.js:1:42)\n", api->get_exception_as_string(scope, true));

    api->close_scope(scope);
}

TEST_F(PApiBaseTest, SetToGlobal) {
    auto scope = api->open_scope(env_ref);
    auto env = api->get_env_from_ref(env_ref);

    auto g = api->global(env);
    api->set_property(env, g, "SetToGlobal", api->create_int32(env, 123));

    auto code = " SetToGlobal;";
    auto ret = api->eval(env, (const uint8_t*)(code), strlen(code), "test.js");
    ASSERT_TRUE(ret != nullptr);
    ASSERT_TRUE(api->is_int32(env, ret));
    ASSERT_TRUE(api->get_value_int32(env, ret) == 123);

    api->close_scope(scope);
}


TEST_F(PApiBaseTest, CreateJsFunction) {
    auto scope = api->open_scope(env_ref);
    auto env = api->get_env_from_ref(env_ref);

    auto g = api->global(env);
    api->set_property(env, g, "Bar__", api->create_function(env, Bar, this, nullptr));
    auto code = "Bar__(3344);";
    bar_data = 100;
    auto ret = api->eval(env, (const uint8_t*)(code), strlen(code), "test.js");
    ASSERT_FALSE(api->has_caught(scope));
    EXPECT_EQ(bar_data, 3344);

    api->close_scope(scope);
}


} // namespace qjsimpl
} // namespace pesapi

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

