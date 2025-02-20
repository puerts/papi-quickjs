#include <gtest/gtest.h>
#include "CppObjectMapper.h"
#include <quickjs/quickjs.h>
#include "pesapi.h"
#include "JSClassRegister.h"

namespace pesapi {
namespace qjsimpl {

class CppObjectMapperTest : public ::testing::Test {
public:
    static void Foo(struct pesapi_ffi* apis, pesapi_callback_info info)
    {

    }
protected:
    void SetUp() override {
        env_ref = create_qjs_env();
    }

    void TearDown() override {
        if (env_ref) {
            destroy_qjs_env(env_ref);
        }
    }

    pesapi_env_ref env_ref;
};

TEST_F(CppObjectMapperTest, CreateAndDestroyMultQjsEnv) {
    //多次调用create_qjs_env和destroy_qjs_env
    for (int i = 0; i < 10; i++) {
        pesapi_env_ref env_ref = create_qjs_env();
        destroy_qjs_env(env_ref);
    }
}

TEST_F(CppObjectMapperTest, RegApi) {
    pesapi_property_descriptor properties = pesapi_alloc_property_descriptors(1);
    pesapi_set_method_info(properties, 0, "Foo", true, Foo, NULL, NULL);
    pesapi_define_class("Test", NULL, "Test", NULL, NULL, 1, properties, NULL);

    auto clsDef = puerts::FindClassByID("Test");
    ASSERT_TRUE(clsDef != nullptr);

    ASSERT_TRUE(strcmp(clsDef->Functions[0].Name, "Foo") == 0);

    ASSERT_TRUE(clsDef->Functions[0].Callback == Foo);
}


} // namespace qjsimpl
} // namespace pesapi

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

