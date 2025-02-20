#include <gtest/gtest.h>
#include "CppObjectMapper.h"
#include <quickjs/quickjs.h>

namespace pesapi {
namespace qjsimpl {

class CppObjectMapperTest : public ::testing::Test {
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


} // namespace qjsimpl
} // namespace pesapi

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

