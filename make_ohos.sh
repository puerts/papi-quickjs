if [ -n "$OHOS_NDK" ]; then
    export NDK=${OHOS_NDK}
elif [ -n "$OHOS_NDK_HOME" ]; then
    export NDK=${OHOS_NDK_HOME}
else
    export NDK=~/ohos-sdk/linux/native
fi

export PATH=${NDK}/build-tools/cmake/bin:$PATH

function build() {
    ABI=$1
    BUILD_PATH=build.OHOS.${ABI}
    cmake -H. -DOHOS_STL=c++_shared -B${BUILD_PATH} -DOHOS_ARCH=${ABI} -DOHOS_PLATFORM=OHOS -DCMAKE_TOOLCHAIN_FILE=${NDK}/build/cmake/ohos.toolchain.cmake -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON
    cmake --build ${BUILD_PATH} --config Release
}

build armeabi-v7a
build arm64-v8a
