cmake_minimum_required(VERSION 3.16)
project(PasswordGenerator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W4)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Pre-built libsodium (static library, Release, x64, v143)
set(LIBSODIUM_DIR "C:/Users/stts0/AppData/Local/Temp/opencode/libsodium-src")
set(LIBSODIUM_LIB "${LIBSODIUM_DIR}/bin/x64/Release/v143/static/libsodium.lib")
set(LIBSODIUM_INCLUDE "${LIBSODIUM_DIR}/src/libsodium/include")

if(NOT EXISTS "${LIBSODIUM_LIB}")
    message(FATAL_ERROR "libsodium not found at ${LIBSODIUM_LIB}. Build it first:\n  cmake -P build_libsodium.cmake")
endif()

add_library(libsodium::sodium STATIC IMPORTED GLOBAL)
set_target_properties(libsodium::sodium PROPERTIES
    IMPORTED_LOCATION "${LIBSODIUM_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBSODIUM_INCLUDE}"
    INTERFACE_COMPILE_DEFINITIONS "SODIUM_STATIC"
)

# Core library
add_library(pwgen_core
    src/core/SecureBuffer.cpp
    src/core/EntropyAccumulator.cpp
    src/core/StatelessGenerator.cpp
)
target_include_directories(pwgen_core PUBLIC src)
target_link_libraries(pwgen_core PUBLIC libsodium::sodium)

# Executables
add_executable(pwgen src/main.cpp)
target_link_libraries(pwgen PRIVATE pwgen_core)

add_executable(pwgen_tests tests/core_tests.cpp)
target_link_libraries(pwgen_tests PRIVATE pwgen_core)

# -------------------------------------------------------------------
# GUI (Dear ImGui + Win32 + D3D11)
# -------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
)
set(IMGUI_SKIP_LOADER ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SRC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp"
)

add_executable(pwgen_gui
    src/gui/gui_app.cpp
    src/gui/app.rc
    ${IMGUI_SRC}
)
target_compile_definitions(pwgen_gui PRIVATE IMGUI_USE_WCHAR32)
target_include_directories(pwgen_gui PRIVATE "${imgui_SOURCE_DIR}")
target_include_directories(pwgen_gui PRIVATE "${imgui_SOURCE_DIR}/backends")
target_link_libraries(pwgen_gui PRIVATE pwgen_core)
target_link_libraries(pwgen_gui PRIVATE d3d11 dxgi)

# Link in the Win32 GUI subsystem (no console window)
set_target_properties(pwgen_gui PROPERTIES
    WIN32_EXECUTABLE TRUE
)
