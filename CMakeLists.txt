cmake_minimum_required(VERSION 3.2)
project(uTox LANGUAGES C)

#####################
## Project Metadata #
#####################

set(PROJECT_VERSION_MAJOR "0")
set(PROJECT_VERSION_MINOR "15")
set(PROJECT_VERSION_PATCH "0")
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set(PROJECT_COPYRIGHT "Copyleft 2017 uTox contributors. Some rights reserved.")


########################
# Set helper-variables #
########################

# OS
if(CYGWIN)
    # Cygwin thinks it's actually Linux.
    set(WIN32 TRUE)
    set(UNIX FALSE)
endif()

if(WIN32)
    set(WINDOWS TRUE)
elseif(UNIX)
    if(CMAKE_SYSTEM_NAME MATCHES ".*Linux")
        set(LINUX TRUE)
    elseif(CMAKE_SYSTEM_NAME MATCHES "kOpenBSD.*|OpenBSD.*")
        set(OPENBSD TRUE)
    elseif(CMAKE_SYSTEM_NAME MATCHES "kNetBSD.*|NetBSD.*")
        set(NETBSD TRUE)
    elseif(CMAKE_SYSTEM_NAME MATCHES "kFreeBSD.*|FreeBSD")
        set(FREEBSD TRUE)
    endif()
endif()

# 32 or 64 bit
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH_64 TRUE)
else()
  set(ARCH_64 FALSE)
endif()


###########
# Options #
###########

if (UTOX_STATIC AND WINDOWS)
    set(ASAN_DEFAULT OFF)
else()
    set(ASAN_DEFAULT ON)
endif()

option(ENABLE_ASAN          "Enable Address Sanitizer on debug builds"          ${ASAN_DEFAULT})
option(ENABLE_TESTS         "Whether to build test binaries (currently linux only)"         ON )
option(ENABLE_WERROR        "Error on Warning, whether to put -Werror flag to the compiler" OFF)
option(ENABLE_FILTERAUDIO   "Enable Filter Audio"                                           ON )

option(UTOX_STATIC          "Link uTox statically"                                          OFF)
option(TOXCORE_STATIC       "Build uTox with the static version of Toxcore"                 OFF)


#################################
# Include toolchain if required #
#################################

if(WINDOWS AND NOT CROSS_COMPILING)
    include(cmake/win.cmake)

    if(ARCH_64)
        # Helping Cygwin out again..
        set(WIN64 TRUE)
    endif()
elseif(FREEBSD)
    include(cmake/toolchain-freebsd.cmake)
elseif(OPENBSD OR NETBSD)
    include(cmake/toolchain-openbsd.cmake)
endif()


##################
# CMake Settings #
##################

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

# make version available in C files
configure_file(${uTox_SOURCE_DIR}/src/branding.h.in
               ${uTox_SOURCE_DIR}/src/branding.h)

if(${CMAKE_BUILD_TYPE} MATCHES Debug)
    execute_process(COMMAND git describe --abbrev=8 --dirty --always --tags
                    OUTPUT_VARIABLE GIT_VERSION)
    string(REPLACE "\n" " " GIT_VERSION ${GIT_VERSION})
endif()


####################
# C compiler flags #
####################

set(CMAKE_C_STANDARD 11) # this requires at least cmake 3.1

include(CheckCCompilerFlag)

# add compiler flag for all build types
function(add_cflag flag)
    string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" var ${flag})
    if(NOT DEFINED HAVE_C${var})
        message(STATUS "Checking for flag: ${flag}")
    endif()
    set(CMAKE_REQUIRED_QUIET TRUE)

    check_c_compiler_flag("${flag}" HAVE_C${var} QUIET)
    if(HAVE_C${var})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}" PARENT_SCOPE)
    else()
        message(WARNING "Unable to find flag: ${flag}")
    endif()
endfunction()

# Grab environment CFLAGS.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} $ENV{CFLAGS}")

## Add all warning flags we can.
add_cflag("-Wall")
add_cflag("-Wextra")
add_cflag("-Wpointer-arith")

## Warnings we don't tolerate
add_cflag("-Werror=implicit-function-declaration")

## Warnings we don't care about
add_cflag("-Wformat=0")
add_cflag("-Wno-misleading-indentation")

## Everything Else
add_cflag("-fno-strict-aliasing")
add_cflag("-fPIC")

## Optimization Flags

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os")

# Fix GNU stack
if(CMAKE_C_COMPILER_ID MATCHES "GNU")

    if(LINUX)
        add_cflag("-flto")

        set(CMAKE_AR "gcc-ar")
        set(CMAKE_RANLIB "gcc-ranlib")

        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,noexecstack")
    endif()
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_cflag("-flto")

    add_cflag("-Wno-missing-field-initializers")

    if(NOT ENABLE_ASAN)
        add_cflag("-fsanitize=safe-stack")
    endif()
else()
    message(SEND_ERROR "Unknown compiler name")
endif()

# set define for GIT_VERSION
if(GIT_VERSION)
    add_cflag("-DGIT_VERSION='\"${GIT_VERSION}\"'")
endif()


#################
# Build options #
#################

# sanitize memory issues in Debug builds
# https://gcc.gnu.org/gcc-4.8/changes.html
# https://github.com/google/sanitizers/wiki/AddressSanitizer
if (ENABLE_ASAN)
    add_cflag("-fsanitize=address")
    add_cflag("-fno-omit-frame-pointer")
endif()

if(ENABLE_WERROR)
    add_cflag("-Werror")
endif()

if(ENABLE_FILTERAUDIO)
    add_cflag("-DAUDIO_FILTERING")
endif()

if(UTOX_STATIC)
    if(UNIX)
        add_cflag("-Wl -Bstatic -lopus -lopenal -lfilteraudio -Bdynamic -lrt -lm -lpthread -ldl")
    else()
        add_cflag("-static")
    endif()
endif()


if(TOXCORE_STATIC)
    set(TOX_LIBS
        toxencryptsave
        toxav
        toxcore
        toxgroup
        toxmessenger
        toxfriends
        toxdht
        toxnetcrypto
        toxcrypto
        toxnetwork
        )
else()
    set(TOX_LIBS
        toxencryptsave
        toxav
        toxcore
        )
endif()

if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(GUI_TYPE WIN32)
    endif()

    include_directories(SYSTEM libs/toktok/include libs/windows-x64/include/)
    if(WIN64)
        link_directories(libs/toktok/lib /usr/x86_64-w64-mingw32/lib/ libs/windows-x64/lib)
    else()
        link_directories(libs/toktok_32/lib /usr/i686-w64-mingw32/lib/ libs/windows-x32/lib)
    endif()

    ## Needed to build the widows icon
    enable_language(RC)
    set(WINDOWS_ICON src/windows/utox.rc)

    add_subdirectory(src/windows)
elseif(UNIX)
    add_subdirectory(src/xlib)

    if(LINUX)
        # Required for v4l (at least in the Linux static builds on Jenkins)
        set(LIBRARIES "rt")
    endif()
endif()

# include utoxLAYOUT
add_subdirectory(src/layout)
# include utoxUI
add_subdirectory(src/ui)
# include utoxAV
add_subdirectory(src/av)


#############
# uTox main #
#############

add_executable(utox ${GUI_TYPE}
    src/avatar.c
    src/chatlog.c
    src/chrono.c
    src/command_funcs.c
    src/commands.c
    src/file_transfers.c
    src/filesys.c
    src/flist.c
    src/friend.c
    src/groups.c
    src/inline_video.c
    src/main.c
    src/messages.c
    src/notify.c
    src/screen_grab.c
    src/self.c
    src/settings.c
    src/sized_string.h
    src/stb.c
    src/text.c
    src/theme.c
    src/tox.c
    src/tox_callbacks.c
    src/ui.c
    src/ui_i18n.c
    src/utox.c
    src/window.c

    ${WINDOWS_ICON}
    )

target_link_libraries(utox
        utoxAV        utoxNATIVE      utoxUI
        ${TOX_LIBS}   ${LIBRARIES}    sodium
        vpx           pthread         m )

set_property(TARGET utox PROPERTY C_STANDARD 11)
if(UNIX)
    install(TARGETS utox
        RUNTIME DESTINATION "bin"
        )
endif()


###########
# Testing #
###########

if(ENABLE_TESTS)
  if(UNIX)
    # tests currently only run on linux
    enable_testing()
    add_subdirectory(tests)
  endif()
endif()


##############
# Build info #
##############

# print a summary of build options
message("Build options:")
message("----------------------------------")
message("- Add ASAN:                ${ENABLE_ASAN}")
message("- Error on Warning:        ${ENABLE_WERROR}")
message("- Filter Audio:            ${ENABLE_FILTERAUDIO}")
message("- uTox Static:             ${UTOX_STATIC}")
message("- Toxcore Static:          ${TOXCORE_STATIC}")
message("-- Platform Options --------------")
message("- Enable Tests             ${ENABLE_TESTS}")

message("* CMAKE SYSTEM is '${CMAKE_SYSTEM_NAME}'")
message("* C Compiler is '${CMAKE_C_COMPILER}' with the following flags:")
message("* C flags for Debug:       ${CMAKE_C_FLAGS_DEBUG}")
message("* C flags for Release:     ${CMAKE_C_FLAGS_RELEASE}")
message("* C flags for all types:   ${CMAKE_C_FLAGS}")
