function CheckSuccess($message) {
    if (!$?) { throw $message }
}

mkdir -Force deps

$SDL_MAJOR_VERSION = (${env:SDL_VERSION} -split '\.')[0]

if ("${env:COMPILER}" -eq "msvc") {
    if ((Test-Path deps/SDL.zip) -and (Test-Path deps/SDL_mixer.zip)) {
        echo "Using cached SDL libraries"
    } else {
        echo "Downloading SDL"
        curl "https://libsdl.org/release/SDL${SDL_MAJOR_VERSION}-devel-${env:SDL_VERSION}-VC.zip" -o deps/SDL.zip
        CheckSuccess("Download SDL")
        curl "https://libsdl.org/projects/SDL_mixer/release/SDL${SDL_MAJOR_VERSION}_mixer-devel-${env:SDL_MIXER_VERSION}-VC.zip" -o deps/SDL_mixer.zip
        CheckSuccess("Download SDL mixer")
    }
    $SDL_EXT_DIR = Join-Path $PWD.Path "ext\SDL${SDL_MAJOR_VERSION}"
    mkdir -Force $SDL_EXT_DIR

    echo "Extracting SDL"
    7z x deps\SDL.zip "-o$SDL_EXT_DIR"
    CheckSuccess("Unpack SDL")
    7z x deps\SDL_mixer.zip "-o$SDL_EXT_DIR"
    CheckSuccess("Unpack SDL mixer")
} elseif ("${env:COMPILER}" -eq "msvc-arm64") {
    $SDL_DIR = $PWD.Path + "\deps\SDL${SDL_MAJOR_VERSION}"
    mkdir -Force $SDL_DIR
    $SDL_MIXER_DIR = $PWD.Path + "\deps\SDL${SDL_MAJOR_VERSION}_mixer"
    mkdir -Force $SDL_MIXER_DIR
    if ((Test-Path "$SDL_DIR\SDL${SDL_MAJOR_VERSION}.dll") -and (Test-Path "$SDL_MIXER_DIR\SDL${SDL_MAJOR_VERSION}_mixer.dll")) {
        echo "Using cached SDL libraries"
    } else {
        echo "Downloading SDL and SDL_mixer Sources"
        curl "https://libsdl.org/release/SDL${SDL_MAJOR_VERSION}-${env:SDL_VERSION}.zip" -o SDL.zip
        CheckSuccess("Download SDL")
        curl "https://libsdl.org/projects/SDL_mixer/release/SDL${SDL_MAJOR_VERSION}_mixer-${env:SDL_MIXER_VERSION}.zip" -o SDL_mixer.zip
        CheckSuccess("Download SDL mixer")

        7z x SDL.zip
        7z x SDL_mixer.zip

        CheckSuccess("Unpack SDL and SDL mixer")

        cd "SDL${SDL_MAJOR_VERSION}-${env:SDL_VERSION}"
        mkdir build
        cd build

        cmake -G "Visual Studio 17 2022" -A ARM64 -DSDL_LIBC=ON -DCMAKE_BUILD_TYPE=Release ..
        cmake --build . -j 4 --config Release
        cmake --install build --config Release --prefix $SDL_DIR

        CheckSuccess("Build SDL")

        cd ..\..
        cd "SDL${SDL_MAJOR_VERSION}_mixer-${env:SDL_MIXER_VERSION}"
        mkdir build
        cd build
          
        cmake -G "Visual Studio 17 2022" -A ARM64 -DCMAKE_PREFIX_PATH=$SDL_DIR -DCMAKE_BUILD_TYPE=Release -DSDL${SDL_MAJOR_VERSION}MIXER_MP3=ON -DSDL${SDL_MAJOR_VERSION}MIXER_MP3_MINIMP3=ON -DSDL${SDL_MAJOR_VERSION}MIXER_VENDORED=OFF -DSDL${SDL_MAJOR_VERSION}MIXER_SAMPLES=OFF -DSDL${SDL_MAJOR_VERSION}MIXER_FLAC=OFF -DSDL${SDL_MAJOR_VERSION}MIXER_CMD=OFF -DSDL${SDL_MAJOR_VERSION}MIXER_MOD=OFF -DSDL${SDL_MAJOR_VERSION}MIXER_MIDI=OFF -DSDL${SDL_MAJOR_VERSION}MIXER_MIDI_TIMIDITY=OFF -DSDL${SDL_MAJOR_VERSION}MIXER_OPUS=OFF -DSDL${SDL_MAJOR_VERSION}MIXER_VORBIS=STB -DSDL${SDL_MAJOR_VERSION}MIXER_WAVPACK=OFF ..
        cmake --build . -j 4 --config Release
        cmake --install build --config Release --prefix $SDL_MIXER_DIR

        CheckSuccess("Build SDL mixer")

        cd ..\..
    }

    cp -r $SDL_DIR ext\SDL${SDL_MAJOR_VERSION}
    cp -r $SDL_MIXER_DIR ext\SDL${SDL_MAJOR_VERSION}
} else {
    if ((Test-Path deps/SDL.tar.gz) -and (Test-Path deps/SDL_mixer.tar.gz)) {
        echo "Using cached SDL libraries"
    } else {
        echo "Downloading SDL"
        curl "https://libsdl.org/release/SDL${SDL_MAJOR_VERSION}-devel-${env:SDL_VERSION}-mingw.tar.gz" -o deps/SDL.tar.gz
        CheckSuccess("Download SDL")
        curl "https://libsdl.org/projects/SDL_mixer/release/SDL${SDL_MAJOR_VERSION}_mixer-devel-${env:SDL_MIXER_VERSION}-mingw.tar.gz" -o deps/SDL_mixer.tar.gz
        CheckSuccess("Download SDL mixer")
    }
    echo "Extracting SDL"
    tar -zxf deps\SDL.tar.gz -C ext\SDL${SDL_MAJOR_VERSION}
    CheckSuccess("Unpack SDL")
    tar -zxf deps\SDL_mixer.tar.gz -C ext\SDL${SDL_MAJOR_VERSION}
    CheckSuccess("Unpack SDL mixer")
}
