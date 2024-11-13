mkdir build & pushd build

if "%1" == "1" (
  cmake -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -DQJS_NS=1 -G "Visual Studio 16 2019" -A x64 ..
) else (
  cmake -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON -G "Visual Studio 16 2019" -A x64 ..
)

popd
cmake --build build --config Release
pause
