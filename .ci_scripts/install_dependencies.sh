#!/usr/bin/env bash

set -e

function get_sdl_lib_url {
  local MODULE=$1
  local VERSION=$2
  local EXT=$3
  local PLATFORM=$4
  if [[ "$MODULE" == "SDL${SDL_MAJOR_VERSION}_mixer" ]]
  then
    local SDL_URL_PATH="projects/SDL_mixer/release"
  else
    local SDL_URL_PATH="release"
  fi
  if [ ! -z "$PLATFORM" ]
  then
    SDL_LIB_URL="https://www.libsdl.org/$SDL_URL_PATH/$MODULE-devel-$VERSION-$PLATFORM.$EXT"
  else
    SDL_LIB_URL="https://www.libsdl.org/$SDL_URL_PATH/$MODULE-$VERSION.$EXT"
  fi
  echo "Obtained URL for $MODULE-$VERSION: $SDL_LIB_URL"
}

function install_sdl_lib {
  local MODULE=$1
  local VERSION=$2
  local CONFIGURE_OPTIONS=$3
  if [ ! -z "$4" ]
  then
    local ENV_VARS="export $4"
  else
    local ENV_VARS=$4
  fi
  local FILENAME=deps/$MODULE-$VERSION.tar.gz
  local BUILDDIR=deps/build/$MODULE-$VERSION
  local LIBDIR=deps/$MODULE-$VERSION
  local ROOT=$PWD
  if [ ! -f "$FILENAME" ] || [ ! -d "$LIBDIR" ]
  then
    if [ ! -f "$FILENAME" ]
    then
      echo "Downloading $MODULE-$VERSION"
      get_sdl_lib_url $MODULE $VERSION "tar.gz"
      curl -o "$FILENAME" "$SDL_LIB_URL"
    fi
    echo "Building $MODULE-$VERSION"
    mkdir -p deps/build
    mkdir -p $LIBDIR
    tar -zxf "$FILENAME" -C deps/build
    cd $BUILDDIR
    if [[ "$SDL_MAJOR_VERSION" == "2" ]]
    then
      ($ENV_VARS ; $CONFIGURE_PREFIX ./configure --prefix=$ROOT/$LIBDIR $CONFIGURE_OPTIONS)
    else
      mkdir build
      cd build
      $CMAKE_PREFIX cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$ROOT/$LIBDIR $CONFIGURE_OPTIONS -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF ..
    fi
    $MAKE_PREFIX make -j4
    $MAKE_PREFIX make install
    cd $ROOT
    rm -rf deps/build
  fi
  ln -sf "$ROOT/$LIBDIR" "ext/SDL$SDL_MAJOR_VERSION"
}

function install_sdl_macos {
  local MODULE=$1
  local VERSION=$2
  local FILENAME=deps/$MODULE-$VERSION.dmg
  if [ ! -f "$FILENAME" ]
  then
    get_sdl_lib_url $MODULE $VERSION "dmg"
    curl -o "$FILENAME" "$SDL_LIB_URL"
  fi
  local VOLUME=$(hdiutil attach $FILENAME | grep -o '/Volumes/.*')
  mkdir -p ~/Library/Frameworks
  echo "Installing framework:" "/Volumes/SDL$SDL_MAJOR_VERSION"/*.framework
  cp -rp "$VOLUME"/*.framework ~/Library/Frameworks
  if [ -d "$VOLUME/optional" ]
  then
    echo "Installing optional framework:" "/Volumes/SDL$SDL_MAJOR_VERSION/optional"/*.framework
    cp -rp "$VOLUME"/optional/*.framework ~/Library/Frameworks
  fi
  hdiutil detach "$VOLUME"
}

function install_sdl_android {
  local MODULE=$1
  local VERSION=$2
  local FILENAME=ext/SDL$SDL_MAJOR_VERSION/$MODULE-$VERSION.tar.gz
  get_sdl_lib_url $MODULE $VERSION "tar.gz"
  curl -o "$FILENAME" "$SDL_LIB_URL"
  tar -zxf $FILENAME -C "ext/SDL$SDL_MAJOR_VERSION"
  rm $FILENAME
}

function install_sdl_android_aar {
  local MODULE=$1
  local VERSION=$2
  local ZIP_FILENAME="$MODULE-devel-$VERSION-android.zip"
  local AAR_FILENAME="$MODULE-$VERSION.aar"

  if [ ! -f "deps/$AAR_FILENAME" ]
  then
    get_sdl_lib_url $MODULE $VERSION "zip" "android"
    curl -L -o "$ZIP_FILENAME" "$SDL_LIB_URL"
    unzip -j -o "$ZIP_FILENAME" "$AAR_FILENAME" -d deps
  fi
}

function install_sdl_ios {
  local MODULE=$1
  local VERSION=$2
  local DIRNAME=deps/$MODULE-$VERSION
  local FILENAME=$DIRNAME.tar.gz
  if [ ! -f "$FILENAME" ]
  then
    get_sdl_lib_url $MODULE $VERSION "tar.gz"
    curl -o "$FILENAME" "$SDL_LIB_URL"
  fi
  tar -zxf $FILENAME -C "ext/SDL$SDL_MAJOR_VERSION"
}

SDL_MAJOR_VERSION="${SDL_VERSION%%.*}"

mkdir -p deps
if [ "$BUILD_TARGET" == "appimage" ] || [ "$BUILD_TARGET" == "codeql-cpp" ]
then
  sudo add-apt-repository universe && sudo apt-get update && sudo apt-get -y install libgl1-mesa-dev libsdl2-dev libsdl2-mixer-dev libfuse2
elif [ "$BUILD_TARGET" == "flatpak" ]
then
  sudo apt-get update && sudo apt-get -y install flatpak-builder
  sudo flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
  sudo flatpak-builder repo res/com.github.keriew.augustus.json --install-deps-from=flathub --install-deps-only --delete-build-dirs
  sudo rm -R .flatpak-builder
elif [ ! -z "$SDL_VERSION" ] && [ ! -z "$SDL_MIXER_VERSION" ]
then
  if [ "$BUILD_TARGET" == "mac" ]
  then
    install_sdl_macos "SDL$SDL_MAJOR_VERSION" $SDL_VERSION
    install_sdl_macos "SDL${SDL_MAJOR_VERSION}_mixer" $SDL_MIXER_VERSION
  elif [ "$BUILD_TARGET" == "android" ]
  then
    if [[ "$SDL_MAJOR_VERSION" == "3" ]]
    then
      install_sdl_android_aar "SDL$SDL_MAJOR_VERSION" $SDL_VERSION
      install_sdl_android_aar "SDL${SDL_MAJOR_VERSION}_mixer" $SDL_MIXER_VERSION
      mkdir -p android/augustus/libs
      cp "deps/SDL$SDL_MAJOR_VERSION-$SDL_VERSION.aar" "android/augustus/libs/SDL$SDL_MAJOR_VERSION.aar"
      cp "deps/SDL${SDL_MAJOR_VERSION}_mixer-$SDL_MIXER_VERSION.aar" "android/augustus/libs/SDL${SDL_MAJOR_VERSION}_mixer.aar"
    else
  	  if [ ! -f "android/augustus.keystore" ]
      then
        BUILDTYPE=debug
      else
        BUILDTYPE=release
      fi
      if [ ! -f "deps/SDL$SDL_MAJOR_VERSION-$BUILDTYPE.aar" ]
      then
        install_sdl_android "SDL$SDL_MAJOR_VERSION" $SDL_VERSION
        install_sdl_android "SDL${SDL_MAJOR_VERSION}_mixer" $SDL_MIXER_VERSION
      else
        mkdir android/augustus/libs
        cp deps/SDL$SDL_MAJOR_VERSION-$BUILDTYPE.aar android/augustus/libs/SDL$SDL_MAJOR_VERSION-$BUILDTYPE.aar
      fi
    fi
  elif [ "$BUILD_TARGET" == "ios" ]
  then
    install_sdl_ios "SDL$SDL_MAJOR_VERSION" $SDL_VERSION
    install_sdl_ios "SDL${SDL_MAJOR_VERSION}_mixer" $SDL_MIXER_VERSION
  else
    if [ "$BUILD_TARGET" == "emscripten" ]
    then
      source ${PWD}/emsdk/emsdk_env.sh
      CONFIGURE_PREFIX="emconfigure"
      MAKE_PREFIX="emmake"
      CMAKE_PREFIX="emcmake"
      SDL_CONFIGURE_OPTIONS="--host=wasm32-unknown-emscripten --disable-assembly --disable-cpuinfo"
      SDL_MIXER_CONFIGURE_OPTIONS="--host=wasm32-unknown-emscripten"
    fi
    if [[ "$SDL_MAJOR_VERSION" == "3" ]]
    then
      sudo apt-get update && sudo apt-get -y install libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
        libxfixes-dev libxi-dev libxss-dev libxtst-dev \
        libxkbcommon-dev libpulse-dev libaudio-dev
      SDL_CONFIGURE_OPTIONS=""
      SDL_MIXER_CONFIGURE_OPTIONS="-DCMAKE_PREFIX_PATH=$PWD/deps/SDL3-$SDL_VERSION"
    fi
    install_sdl_lib "SDL$SDL_MAJOR_VERSION" $SDL_VERSION "$SDL_CONFIGURE_OPTIONS"
    install_sdl_lib "SDL${SDL_MAJOR_VERSION}_mixer" $SDL_MIXER_VERSION "$SDL_MIXER_CONFIGURE_OPTIONS" \
      "SDL2_CONFIG=$PWD/deps/SDL2-$SDL_VERSION/bin/sdl2-config"
  fi
fi
