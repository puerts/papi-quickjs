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

    static void JsFuncFinalizer(struct pesapi_ffi* apis, void* data, void* env_private)
    {
        PApiBaseTest* self = (PApiBaseTest*)data;
        self->finalizer_env_private = env_private;
    }

    int bar_data = 0;

    void* finalizer_env_private = nullptr;

protected:
    void SetUp() override {
        //printf("SetUp\n");
        env_ref = create_qjs_env();
        api = get_papi_ffi();

        auto scope = api->open_scope(env_ref);
        auto env = api->get_env_from_ref(env_ref);

        auto g = api->global(env);
        api->set_property(env, g, "loadClass", api->create_function(env, LoadClass, this, nullptr));
        api->close_scope(scope);
    }

    static void LoadClass(struct pesapi_ffi* apis, pesapi_callback_info info)
    {
        auto env = apis->get_env(info);
        auto arg0 = apis->get_arg(info, 0);
        if (apis->is_string(env, arg0))
        {
            char buff[1024];
            size_t len = sizeof(buff);
            const char* className = apis->get_value_string_utf8(env, arg0, buff, &len);
            printf("LoadClass className: %s\n", className);
            auto clsDef = puerts::FindCppTypeClassByName(className);
            if (clsDef)
            {
                auto ret = apis->create_class(env, clsDef->TypeId);
                apis->add_return(info, ret);
            }
            else
            {
                printf("LoadClass className: %s fail!!!\n", className);
            }
        }
    }

    void TearDown() override {
        //printf("TearDown\n");
        if (env_ref) {
            destroy_qjs_env(env_ref);
        }
    }

    pesapi_env_ref env_ref;
    struct pesapi_ffi* api;
};

TEST_F(PApiBaseTest, CreateAndDestroyMultQjsEnv) {
    for (int i = 0; i < 10; i++) {
       pesapi_env_ref env_ref = create_qjs_env();
        destroy_qjs_env(env_ref);
    }
}

TEST_F(PApiBaseTest, RegApi) {
    const void* typeId = "Test";

    pesapi_property_descriptor properties = pesapi_alloc_property_descriptors(1);
    pesapi_set_method_info(properties, 0, "Foo", true, Foo, NULL, NULL);
    pesapi_define_class(typeId, NULL, "Test", NULL, NULL, 1, properties, NULL);

    auto clsDef = puerts::FindClassByID(typeId);
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
    api->set_property(env, g, "Bar__", api->create_function(env, Bar, this, JsFuncFinalizer));
    auto code = "Bar__(3344);";
    bar_data = 100;
    auto ret = api->eval(env, (const uint8_t*)(code), strlen(code), "test.js");
    if (api->has_caught (scope)) {
        printf("%s\n", api->get_exception_as_string(scope, true));
    }
    ASSERT_FALSE(api->has_caught(scope));
    EXPECT_EQ(bar_data, 3344);

    code = "globalThis.Bar__ = undefined;";
    finalizer_env_private = nullptr;
    api->set_env_private(env, &bar_data);
    ret = api->eval(env, (const uint8_t*)(code), strlen(code), "test2.js");
    ASSERT_FALSE(api->has_caught(scope));

    api->close_scope(scope);

    EXPECT_EQ((void*)&bar_data, finalizer_env_private);
    EXPECT_EQ(api->get_env_private(env), finalizer_env_private);
    api->set_env_private(env, nullptr);
}


TEST_F(PApiBaseTest, PropertyGetSet) {
    auto scope = api->open_scope(env_ref);
    auto env = api->get_env_from_ref(env_ref);

    auto g = api->global(env);
    api->set_property(env, g, "PropertyGetSet", api->create_string_utf8(env, "123", 3));

    auto str = api->get_property(env, g, "PropertyGetSet");
    ASSERT_TRUE(api->is_string(env, str));
    size_t len = 0;
    api->get_value_string_utf8(env, str, nullptr, &len);
    ASSERT_EQ(len, 3);
    char buff[4] = {0};
    api->get_value_string_utf8(env, str, buff, &len);
    buff[3] = 0;
    EXPECT_STREQ("123", buff);

    api->set_property_uint32(env, g, 5, api->create_string_utf8(env, "888", 3));
    str = api->get_property_uint32(env, g, 5);
    ASSERT_TRUE(api->is_string(env, str));
    len = 0;
    api->get_value_string_utf8(env, str, nullptr, &len);
    ASSERT_EQ(len, 3);
    buff[3] = 0;
    api->get_value_string_utf8(env, str, buff, &len);
    EXPECT_STREQ("888", buff);

    api->close_scope(scope);
}

struct TestStruct {
    TestStruct(int a) {
        printf("TestStruct ctor: %d, %p\n", a, this);
        this->a = a;
    }

    int a;
    ~TestStruct() {
        printf("TestStruct dtor: %d, %p\n", a, this);
    }
};

void* TestStructCtor(struct pesapi_ffi* apis, pesapi_callback_info info)
{
    auto env = apis->get_env(info);
    auto p0 = apis->get_arg(info, 0);
    int a = apis->get_value_int32(env, p0);
    return new TestStruct(a);
}

void TestStructFinalize(struct pesapi_ffi* apis, void* ptr, void* class_data, void* env_private)
{
    delete (TestStruct*)ptr;
}

TEST_F(PApiBaseTest, ClassCtorFinalize) {
    auto scope = api->open_scope(env_ref);
    auto env = api->get_env_from_ref(env_ref);

    const void* typeId = "TestStruct";

    pesapi_define_class(typeId, NULL, "TestStruct", TestStructCtor, TestStructFinalize, 0, NULL, NULL);

    auto code = R"(
                (function() {
                    const TestStruct = loadClass('TestStruct');
                    const obj = new TestStruct(123);
                })();
              )";
    api->eval(env, (const uint8_t*)(code), strlen(code), "test.js");
    if (api->has_caught(scope))
    {
        printf("%s\n", api->get_exception_as_string(scope, true));
    }
    ASSERT_FALSE(api->has_caught(scope));

    api->close_scope(scope);
}


} // namespace qjsimpl
} // namespace pesapi

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

