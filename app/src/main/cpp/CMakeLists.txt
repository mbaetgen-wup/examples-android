# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.
cmake_minimum_required(VERSION 3.22.1)
project(projectm_android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Install/staging prefix inside this ABI build directory
set(PROJECTM_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/projectm-install" CACHE PATH "")

# Force install prefix so projectM installs headers into:
#   ${PROJECTM_INSTALL_PREFIX}/include/projectM-4/...
set(CMAKE_INSTALL_PREFIX "${PROJECTM_INSTALL_PREFIX}" CACHE PATH "" FORCE)


# ---- Fetch OR use local libprojectM (projectM 4.x) ----
include(FetchContent)

# If set, use local source instead of fetching.
# Example: -DPROJECTM_SOURCE_DIR=/home/mba/dev/widerup/projectm
set(PROJECTM_SOURCE_DIR "" CACHE PATH "Path to local projectM source directory (with top-level CMakeLists.txt)")

# Configure projectM build options (mirrors upstream Android workflow).
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_TESTING NO CACHE BOOL "" FORCE)
set(ENABLE_SDL_UI OFF CACHE BOOL "" FORCE)

if (PROJECTM_SOURCE_DIR AND EXISTS "${PROJECTM_SOURCE_DIR}/CMakeLists.txt")
    message(STATUS "Using local projectM source: ${PROJECTM_SOURCE_DIR}")

    # FetchContent will treat SOURCE_DIR as the content and won't hit the network.
    FetchContent_Declare(
        projectm
        SOURCE_DIR "${PROJECTM_SOURCE_DIR}"
    )
else()
    message(STATUS "Fetching projectM via Git (PROJECTM_SOURCE_DIR not set or invalid)")

    FetchContent_Declare(
        projectm
        GIT_REPOSITORY https://github.com/projectM-visualizer/projectm.git
        GIT_TAG        master
    )
endif()

FetchContent_MakeAvailable(projectm)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

# ---- JNI wrapper ----
add_library(jniwrapper SHARED
    jni.cpp
)

# Always produce libjniwrapper.so (no debug postfix)
set_target_properties(jniwrapper PROPERTIES
        OUTPUT_NAME "jniwrapper"
        DEBUG_POSTFIX ""
        RELEASE_POSTFIX ""
        RELWITHDEBINFO_POSTFIX ""
        MINSIZEREL_POSTFIX ""
)

# Also guard against a global postfix set by a toolchain / parent project
set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "" FORCE)

# find projectM headers
if (DEFINED projectm_SOURCE_DIR)
    target_include_directories(jniwrapper PRIVATE
        "${projectm_SOURCE_DIR}/src/playlist/api"
    )
endif()

if (DEFINED projectm_BINARY_DIR)
    target_include_directories(jniwrapper PRIVATE
        "${projectm_BINARY_DIR}/src/api/include"
        "${projectm_BINARY_DIR}/src/playlist/include"
    )
else()
    message(FATAL_ERROR "projectm_BINARY_DIR not defined; cannot set build-tree include paths")
endif()

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

# Android + GLES
find_library(log-lib log)
find_library(android-lib android)
find_library(glesv2-lib GLESv2)
find_library(egl-lib EGL)

target_include_directories(jniwrapper
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(jniwrapper
    PRIVATE
        ${log-lib}
        ${android-lib}
        ${glesv2-lib}
        ${egl-lib}
        libprojectM::playlist
        projectM
)
