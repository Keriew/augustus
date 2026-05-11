#!/usr/bin/env bash

set -e

if [ -n "$SDL_VERSION" ]
then
  SDL_MAJOR_VERSION=${SDL_VERSION%%.*}

  if [[ "$SDL_MAJOR_VERSION" == "3" ]]
  then
	CMAKE_SDL_VERSION="-DSDL_VERSION=3"
	CMAKE_SDL_PREFIX="-DCMAKE_PREFIX_PATH=$PWD/ext/SDL3/SDL3-$SDL_VERSION;$PWD/ext/SDL3/SDL3_mixer-$SDL_MIXER_VERSION"
  fi
fi

case "$BUILD_TARGET" in
"vita")
	docker exec vitasdk /bin/bash -c "git config --global --add safe.directory /build/git && mkdir build && cd build && cmake -DAV1_VIDEO_SUPPORT=ON -DCMAKE_BUILD_TYPE=Release $CMAKE_SDL_VERSION -DTARGET_PLATFORM=vita .."
	;;
"switch")
	docker exec switchdev /bin/bash -c "git config --global --add safe.directory /build/git && /opt/devkitpro/portlibs/switch/bin/aarch64-none-elf-cmake -DAV1_VIDEO_SUPPORT=ON -DCMAKE_BUILD_TYPE=Release $CMAKE_SDL_VERSION -DTARGET_PLATFORM=switch -B build -S ."
	;;
"mac")
	mkdir build && cd build && cmake -DAV1_VIDEO_SUPPORT=ON -DCMAKE_BUILD_TYPE=Release $CMAKE_SDL_VERSION -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" ..
	;;
"ios")
	mkdir build && cd build && cmake -DAV1_VIDEO_SUPPORT=ON -DCMAKE_BUILD_TYPE=Release $CMAKE_SDL_VERSION -DTARGET_PLATFORM=ios -G Xcode ..
	;;
"flatpak")
	;;
"appimage")
	mkdir build && cd build && cmake -DAV1_VIDEO_SUPPORT=ON -DCMAKE_BUILD_TYPE=Release $CMAKE_SDL_VERSION -DCMAKE_INSTALL_PREFIX=/usr ..
	;;
"linux")
	mkdir build && cd build && cmake -DAV1_VIDEO_SUPPORT=ON -DCMAKE_BUILD_TYPE=Release $CMAKE_SDL_VERSION "$CMAKE_SDL_PREFIX" ..
	;;
"android")
	mkdir build
	;;
"emscripten")
	export EMSDK=${PWD}/emsdk
	mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release $CMAKE_SDL_VERSION "$CMAKE_SDL_PREFIX" -DTARGET_PLATFORM=emscripten ..
	;;
*)
	mkdir build && cd build && cmake -DAV1_VIDEO_SUPPORT=ON $CMAKE_SDL_VERSION "$CMAKE_SDL_PREFIX" ..
	;;
esac
