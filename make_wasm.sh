git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
git pull
./emsdk install 3.1.8
./emsdk activate 3.1.8
source ./emsdk_env.sh
cd ..

mkdir -p build_wasm && cd build_wasm
emcmake cmake -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON ../
emmake make
