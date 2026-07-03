# build_libsodium.cmake
# Download and build libsodium (stable) as a static library for MSVC v143 /MT
#
# Usage: cmake -P build_libsodium.cmake

cmake_minimum_required(VERSION 3.16)

set(LIBSODIUM_VERSION "stable")
set(LIBSODIUM_DIR "${CMAKE_CURRENT_LIST_DIR}/_deps/libsodium-src")
set(LIBSODIUM_OUT "${LIBSODIUM_DIR}/bin/x64/Release/v143/static")

if(EXISTS "${LIBSODIUM_OUT}/libsodium.lib")
    message(STATUS "libsodium already built at ${LIBSODIUM_OUT}")
    return()
endif()

message(STATUS "Downloading libsodium (${LIBSODIUM_VERSION})...")
file(DOWNLOAD
    "https://download.libsodium.org/libsodium/releases/libsodium-1.0.20-stable-msvc.zip"
    "${CMAKE_CURRENT_LIST_DIR}/_deps/libsodium.zip"
    EXPECTED_MD5 941e94a5a9e9b9b4031a7b1a01bdbb38
)

message(STATUS "Extracting...")
file(ARCHIVE_EXTRACT
    INPUT "${CMAKE_CURRENT_LIST_DIR}/_deps/libsodium.zip"
    DESTINATION "${CMAKE_CURRENT_LIST_DIR}/_deps"
)

# Find MSBuild
get_filename_component(MSVC_DIR
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\VisualStudio\\17.0\\Setup\\VS;ProductDir]"
    ABSOLUTE CACHE)

find_program(MSBUILD msbuild.exe PATHS "${MSVC_DIR}/MSBuild/Current/Bin")

if(NOT MSBUILD)
    message(FATAL_ERROR "MSBuild not found. Install Visual Studio 2022 Build Tools.")
endif()

message(STATUS "Building libsodium (StaticRelease, x64)...")
execute_process(
    COMMAND "${MSBUILD}" "libsodium.vcxproj"
        /p:Configuration=StaticRelease
        /p:Platform=x64
        /p:VCTargetsPath="C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Microsoft/VC/v170"
    WORKING_DIRECTORY "${LIBSODIUM_DIR}/builds/msvc/vs2022"
    RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "libsodium build failed")
endif()

message(STATUS "libsodium built successfully at ${LIBSODIUM_OUT}")
