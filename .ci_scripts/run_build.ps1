mkdir -Force build
cd build

$SDL_MAJOR_VERSION = (${env:SDL_VERSION} -split '\.')[0]

if ($SDL_MAJOR_VERSION -eq "3") {
    $CMAKE_SDL_VERSION = "-DSDL_VERSION=3"
    $CMAKE_SDL_PREFIX = "-DCMAKE_PREFIX_PATH=$PWD/../ext/SDL3/SDL3-$env:SDL_VERSION;$PWD/../ext/SDL3/SDL3_mixer-$env:SDL_MIXER_VERSION"
}

if ("${env:COMPILER}" -eq "msvc") {
    cmake -G "Visual Studio 17 2022" -A x64 -DAV1_VIDEO_SUPPORT=ON $CMAKE_SDL_VERSION "$CMAKE_SDL_PREFIX" -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
    cmake --build . -j 4 --config RelWithDebInfo
} elseif ("${env:COMPILER}" -eq "msvc-arm64") {
    cmake -G "Ninja" -DAV1_VIDEO_SUPPORT=ON $CMAKE_SDL_VERSION "$CMAKE_SDL_PREFIX" -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
    ninja
} elseif ("${env:COMPILER}" -eq "mingw-32") {
    $env:path = "D:\msys64\mingw32\bin;${env:path}"
    cmake -G "MinGW Makefiles" -DAV1_VIDEO_SUPPORT=ON $CMAKE_SDL_VERSION "$CMAKE_SDL_PREFIX" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc.exe -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ..
    cmake --build . -j 4 --config RelWithDebInfo
} elseif ("${env:COMPILER}" -eq "mingw-64") {
    cmake -G "MinGW Makefiles" -DAV1_VIDEO_SUPPORT=ON $CMAKE_SDL_VERSION "$CMAKE_SDL_PREFIX" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc.exe -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ..
    cmake --build . -j 4 --config RelWithDebInfo
} else {
    throw "Unknown compiler: ${env:COMPILER}"
}
if (!$?) {
    throw "Build failed"
}
