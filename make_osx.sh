#!/bin/bash
CONFIG=${1:-Release}

mkdir -p build && cd build
cmake -GXcode ../
cd ..
cmake --build build --config $CONFIG
