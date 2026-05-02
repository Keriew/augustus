# Locate SDL3 library
# This module defines
# SDL3_LIBRARY, the name of the library to link against
# SDL3_FOUND, if false, do not try to link to SDL3
# SDL3_INCLUDE_DIR, where to find SDL.h
#
# This module responds to the the flag:
# SDL3_BUILDING_LIBRARY
# If this is defined, then no SDL3main will be linked in because
# only applications need main().
# Otherwise, it is assumed you are building an application and this
# module will attempt to locate and set the the proper link flags
# as part of the returned SDL3_LIBRARY variable.
#
# Don't forget to include SDLmain.h and SDLmain.m your project for the
# OS X framework based version. (Other versions link to -lSDL3main which
# this module will try to find on your behalf.) Also for OS X, this
# module will automatically add the -framework Cocoa on your behalf.
#
#
# Additional Note: If you see an empty SDL3_LIBRARY_TEMP in your configuration
# and no SDL3_LIBRARY, it means CMake did not find your SDL3 library
# (SDL3.dll, libSDL3.so, SDL3.framework, etc).
# Set SDL3_LIBRARY_TEMP to point to your SDL3 library, and configure again.
# Similarly, if you see an empty SDL3MAIN_LIBRARY, you should set this value
# as appropriate. These values are used to generate the final SDL3_LIBRARY
# variable, but when these values are unset, SDL3_LIBRARY does not get created.
#
#
# $SDL3DIR is an environment variable that would
# correspond to the ./configure --prefix=$SDL3DIR
# used in building SDL3.
# l.e.galup  9-20-02
#
# Modified by Eric Wing.
# Added code to assist with automated building by using environmental variables
# and providing a more controlled/consistent search behavior.
# Added new modifications to recognize OS X frameworks and
# additional Unix paths (FreeBSD, etc).
# Also corrected the header search path to follow "proper" SDL guidelines.
# Added a search for SDL3main which is needed by some platforms.
# Added a search for threads which is needed by some platforms.
# Added needed compile switches for MinGW.
#
# On OSX, this will prefer the Framework version (if found) over others.
# People will have to manually change the cache values of
# SDL3_LIBRARY to override this selection or set the CMake environment
# CMAKE_INCLUDE_PATH to modify the search paths.
#
# Note that the header path has changed from SDL3/SDL.h to just SDL.h
# This needed to change because "proper" SDL convention
# is #include "SDL.h", not <SDL3/SDL.h>. This is done for portability
# reasons because not all systems place things in SDL3/ (see FreeBSD).

#=============================================================================
# Copyright 2003-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

function(GET_SDL_EXT_DIR result module)
    if(NOT module STREQUAL "")
        set(module "_${module}")
    endif()
    set(SDL_LOCATION ${PROJECT_SOURCE_DIR}/ext/SDL3)
    file(GLOB children
        RELATIVE ${SDL_LOCATION}
        CONFIGURE_DEPENDS
        ${SDL_LOCATION}/SDL${module}
        ${SDL_LOCATION}/SDL3${module}
        ${SDL_LOCATION}/SDL${module}-*
        ${SDL_LOCATION}/SDL3${module}-*
    )
    foreach(child ${children})
        if(IS_DIRECTORY "${SDL_LOCATION}/${child}")
            set(${result} "${SDL_LOCATION}/${child}" PARENT_SCOPE)
            break()
        endif()
    endforeach()
endfunction()

GET_SDL_EXT_DIR(SDL_EXT_DIR "")

IF("${TARGET_PLATFORM}" STREQUAL "android")
    STRING(TOLOWER ${CMAKE_BUILD_TYPE} ANDROID_BUILD_DIR)
    SET(SDL3_LIBRARY SDL3)
    SET(SDL3_ANDROID_HOOK ${SDL_EXT_DIR}/src/main/android/SDL_android_main.c)
    link_directories(${PROJECT_SOURCE_DIR}/android/SDL3/build/intermediates/ndkBuild/${ANDROID_BUILD_DIR}/obj/local/${ANDROID_ABI})

    SET(SDL3_INCLUDE_DIR_TEMP ${SDL_EXT_DIR}/include)
    FOREACH(CURRENT_INCLUDE_DIR ${SDL3_INCLUDE_DIR_TEMP})
        IF(EXISTS "${CURRENT_INCLUDE_DIR}/SDL_version.h")
            SET(SDL3_INCLUDE_DIR ${CURRENT_INCLUDE_DIR})
            BREAK()
        ENDIF()
    ENDFOREACH()
ELSE()
    IF (CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
        set(SDL3_PROCESSOR_ARCH "arm64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(SDL3_ARCH_64 TRUE)
        set(SDL3_PROCESSOR_ARCH "x64")
    else()
        set(SDL3_ARCH_64 FALSE)
        set(SDL3_PROCESSOR_ARCH "x86")
    endif()

    if(MINGW AND DEFINED SDL_EXT_DIR AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
        if(SDL3_ARCH_64)
            set(SDL_MINGW_EXT_DIR "${SDL_EXT_DIR}/x86_64-w64-mingw32")
        else()
            set(SDL_MINGW_EXT_DIR "${SDL_EXT_DIR}/i686-w64-mingw32")
        endif()
    endif()

    SET(SDL3_SEARCH_PATHS
        ${SDL_EXT_DIR}
        ${SDL_MINGW_EXT_DIR}
        ~/Library/Frameworks
        /Library/Frameworks
        /sw # Fink
        /opt/local # DarwinPorts
        /opt/csw # Blastwave
        /opt
        /boot/system/develop/headers/SDL3 # Haiku
        ${CMAKE_FIND_ROOT_PATH}
    )

    FIND_PATH(SDL3_INCLUDE_DIR "SDL3/SDL_log.h"
        HINTS
        $ENV{SDL3DIR}
        PATH_SUFFIXES include
        PATHS ${SDL3_SEARCH_PATHS}
        NO_CMAKE_FIND_ROOT_PATH
    )

    FIND_LIBRARY(SDL3_LIBRARY_TEMP
        NAMES SDL3
        HINTS
        $ENV{SDL3DIR}
        PATH_SUFFIXES lib64 lib lib/${SDL3_PROCESSOR_ARCH}
        PATHS ${SDL3_SEARCH_PATHS}
        NO_CMAKE_FIND_ROOT_PATH
    )

    IF(NOT SDL3_BUILDING_LIBRARY)
        IF(NOT ${SDL3_INCLUDE_DIR} MATCHES ".framework")
            # Non-OS X framework versions expect you to also dynamically link to
            # SDL3main. This is mainly for Windows and OS X. Other (Unix) platforms
            # seem to provide SDL3main for compatibility even though they don't
            # necessarily need it.
            FIND_LIBRARY(SDL3MAIN_LIBRARY
                NAMES SDL3main
                HINTS
                $ENV{SDL3DIR}
                PATH_SUFFIXES lib64 lib lib/${SDL3_PROCESSOR_ARCH}
                PATHS ${SDL3_SEARCH_PATHS}
            )
        ENDIF(NOT ${SDL3_INCLUDE_DIR} MATCHES ".framework")
    ENDIF(NOT SDL3_BUILDING_LIBRARY)
ENDIF()

# SDL3 may require threads on your system.
# The Apple build may not need an explicit flag because one of the
# frameworks may already provide it.
# But for non-OSX systems, I will use the CMake Threads package.
IF(NOT APPLE AND NOT EMSCRIPTEN)
    FIND_PACKAGE(Threads)
ENDIF()

# MinGW needs an additional library, mwindows
# It's total link flags should look like -lmingw32 -lSDL3main -lSDL3 -lmwindows
# (Actually on second look, I think it only needs one of the m* libraries.)
IF(MINGW)
   # SET(MINGW32_LIBRARY mingw32 CACHE STRING "mwindows for MinGW")
ENDIF(MINGW)

IF(SDL3_LIBRARY_TEMP)
    # For SDL3main
    IF(NOT SDL3_BUILDING_LIBRARY)
        IF(SDL3MAIN_LIBRARY)
            SET(SDL3_LIBRARY_TEMP ${SDL3MAIN_LIBRARY} ${SDL3_LIBRARY_TEMP})
        ENDIF(SDL3MAIN_LIBRARY)
    ENDIF(NOT SDL3_BUILDING_LIBRARY)

    # For OS X, SDL3 uses Cocoa as a backend so it must link to Cocoa.
    # CMake doesn't display the -framework Cocoa string in the UI even
    # though it actually is there if I modify a pre-used variable.
    # I think it has something to do with the CACHE STRING.
    # So I use a temporary variable until the end so I can set the
    # "real" variable in one-shot.
    IF(APPLE)
        SET(SDL3_LIBRARY_TEMP ${SDL3_LIBRARY_TEMP} "-framework Cocoa")
    ENDIF(APPLE)

    # For threads, as mentioned Apple doesn't need this.
    # In fact, there seems to be a problem if I used the Threads package
    # and try using this line, so I'm just skipping it entirely for OS X.
    IF(NOT APPLE)
        SET(SDL3_LIBRARY_TEMP ${SDL3_LIBRARY_TEMP} ${CMAKE_THREAD_LIBS_INIT})
    ENDIF(NOT APPLE)

    # For MinGW library
    IF(MINGW)
        SET(SDL3_LIBRARY_TEMP ${MINGW32_LIBRARY} ${SDL3_LIBRARY_TEMP})
    ENDIF(MINGW)

    # Set the final string here so the GUI reflects the final state.
    SET(SDL3_LIBRARY ${SDL3_LIBRARY_TEMP} CACHE STRING "Where the SDL3 Library can be found")
    # Set the temp variable to INTERNAL so it is not seen in the CMake GUI
    SET(SDL3_LIBRARY_TEMP "${SDL3_LIBRARY_TEMP}" CACHE INTERNAL "")
ENDIF(SDL3_LIBRARY_TEMP)

if(SDL3_INCLUDE_DIR AND EXISTS "${SDL3_INCLUDE_DIR}/SDL3/SDL_version.h")
    file(STRINGS "${SDL3_INCLUDE_DIR}/SDL3/SDL_version.h" SDL3_VERSION_MAJOR_LINE REGEX "^#define[ \t]+SDL_MAJOR_VERSION[ \t]+[0-9]+$")
    file(STRINGS "${SDL3_INCLUDE_DIR}/SDL3/SDL_version.h" SDL3_VERSION_MINOR_LINE REGEX "^#define[ \t]+SDL_MINOR_VERSION[ \t]+[0-9]+$")
    file(STRINGS "${SDL3_INCLUDE_DIR}/SDL3/SDL_version.h" SDL3_VERSION_MICRO_LINE REGEX "^#define[ \t]+SDL_MICRO_VERSION[ \t]+[0-9]+$")
    string(REGEX REPLACE "^#define[ \t]+SDL_MAJOR_VERSION[ \t]+([0-9]+)$" "\\1" SDL3_VERSION_MAJOR "${SDL3_VERSION_MAJOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+SDL_MINOR_VERSION[ \t]+([0-9]+)$" "\\1" SDL3_VERSION_MINOR "${SDL3_VERSION_MINOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+SDL_MICRO_VERSION[ \t]+([0-9]+)$" "\\1" SDL3_VERSION_MICRO "${SDL3_VERSION_MICRO_LINE}")
    set(SDL3_VERSION_STRING ${SDL3_VERSION_MAJOR}.${SDL3_VERSION_MINOR}.${SDL3_VERSION_MICRO})
    unset(SDL3_VERSION_MAJOR_LINE)
    unset(SDL3_VERSION_MINOR_LINE)
    unset(SDL3_VERSION_MICRO_LINE)
    unset(SDL3_VERSION_MAJOR)
    unset(SDL3_VERSION_MINOR)
    unset(SDL3_VERSION_MICRO)
endif()


INCLUDE(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(SDL3
                                  REQUIRED_VARS SDL3_LIBRARY SDL3_INCLUDE_DIR
                                  VERSION_VAR SDL3_VERSION_STRING)
