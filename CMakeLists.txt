cmake_minimum_required(VERSION 3.20)
project(NITRO_FX LANGUAGES CXX)
# Target: GTA San Andreas PC 1.0 US, 32-bit. Configure with:  cmake -S . -B build -A Win32 -DPLUGIN_SDK_DIR=<path to DK22Pac/plugin-sdk>
if(NOT PLUGIN_SDK_DIR AND DEFINED ENV{PLUGIN_SDK_DIR})
  set(PLUGIN_SDK_DIR $ENV{PLUGIN_SDK_DIR})
endif()
file(TO_CMAKE_PATH "${PLUGIN_SDK_DIR}" PLUGIN_SDK_DIR)
if(NOT EXISTS "${PLUGIN_SDK_DIR}/plugin_sa")
  message(FATAL_ERROR "PLUGIN_SDK_DIR must point to a plugin-sdk checkout (folder containing plugin_sa/). Got: '${PLUGIN_SDK_DIR}'")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "GTA SA is a 32-bit process: configure with -A Win32.")
endif()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # static CRT: no vcruntime dependency for the user

# Plugin-SDK sources are compiled directly (no prebuilt plugin.lib needed).
file(GLOB_RECURSE SDK_SRC "${PLUGIN_SDK_DIR}/plugin_sa/*.cpp" "${PLUGIN_SDK_DIR}/shared/*.cpp")
list(FILTER SDK_SRC EXCLUDE REGEX "/(examples|tools|imgui|bass|d3d9|d3dx9)/")
list(LENGTH SDK_SRC SDK_SRC_COUNT)
message(STATUS "plugin-sdk sources found: ${SDK_SRC_COUNT}")

file(GLOB NFX_SRC "${CMAKE_SOURCE_DIR}/src/*.cpp")
add_library(NITRO_FX SHARED ${NFX_SRC} ${SDK_SRC})
set_target_properties(NITRO_FX PROPERTIES PREFIX "" SUFFIX ".asi" OUTPUT_NAME "NITRO_FX")
target_include_directories(NITRO_FX PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${PLUGIN_SDK_DIR}/plugin_sa ${PLUGIN_SDK_DIR}/plugin_sa/game_sa ${PLUGIN_SDK_DIR}/plugin_sa/game_sa/rw
  ${PLUGIN_SDK_DIR}/shared ${PLUGIN_SDK_DIR}/shared/game)
target_compile_definitions(NITRO_FX PRIVATE GTASA PLUGIN_SGV_10US _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE _USING_V110_SDK71_ NOMINMAX_UNUSED)
if(MSVC)
  target_compile_options(NITRO_FX PRIVATE /MP /permissive /W3 /wd4244 /wd4305 /wd4838 /wd4996 /wd4267 /wd4018)
endif()
target_link_libraries(NITRO_FX PRIVATE user32 kernel32)
