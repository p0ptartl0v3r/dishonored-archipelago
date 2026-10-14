cmake_minimum_required(VERSION 3.20)
project(dishonored_archipelago_native CXX C)

# Dishonored is a 32-bit game: build for Win32/x86 only.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Build this as 32-bit (Visual Studio: -A Win32, MinGW: i686-w64-mingw32).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  minhook
  GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
  GIT_TAG        v1.3.3
)
FetchContent_MakeAvailable(minhook)  # no CMake file in this release: just downloads it

add_library(minhook STATIC
  ${minhook_SOURCE_DIR}/src/buffer.c
  ${minhook_SOURCE_DIR}/src/hook.c
  ${minhook_SOURCE_DIR}/src/trampoline.c
  ${minhook_SOURCE_DIR}/src/hde/hde32.c
)
target_include_directories(minhook PUBLIC ${minhook_SOURCE_DIR}/include)

add_library(dinput8 SHARED
  src/dllmain.cpp
  src/config.cpp
  src/log.cpp
  src/memory.cpp
  src/ue3.cpp
  src/game_hooks.cpp
  src/pipe_server.cpp
)
target_link_libraries(dinput8 PRIVATE minhook)
# The game loads dinput8.dll from its own folder before the system one.
set_target_properties(dinput8 PROPERTIES OUTPUT_NAME "dinput8" PREFIX "")

if(MSVC)
  target_sources(dinput8 PRIVATE src/dinput8.def)
  target_compile_options(dinput8 PRIVATE /W4 /permissive-)
  set_property(TARGET dinput8 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  # Export undecorated stdcall names and avoid depending on MinGW runtime DLLs.
  target_link_options(dinput8 PRIVATE -Wl,--kill-at -static -static-libgcc -static-libstdc++ -s)
  target_compile_options(dinput8 PRIVATE -Wall -Wextra)
endif()
