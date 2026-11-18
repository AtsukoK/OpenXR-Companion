cmake_minimum_required(VERSION 3.20)
project(OpenXRHandMenus CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_library(OpenXRHandMenus SHARED
    src/layer.cpp src/engine.cpp src/keyboard.cpp src/wrist.cpp src/config.cpp src/keys.cpp src/panel.cpp src/common.cpp)
target_include_directories(OpenXRHandMenus PRIVATE
    ${CMAKE_SOURCE_DIR}/../third_party/openxr-sdk/include
    ${CMAKE_SOURCE_DIR}/../third_party)
target_compile_definitions(OpenXRHandMenus PRIVATE UNICODE _UNICODE)
target_compile_options(OpenXRHandMenus PRIVATE /W3 /EHsc /utf-8)
target_link_libraries(OpenXRHandMenus PRIVATE d3d11 d3d12 gdiplus shell32 ole32 user32)
# 32-bit: __stdcall decorates the export (_name@12); the OpenXR loader looks it up undecorated
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_link_options(OpenXRHandMenus PRIVATE
        "/EXPORT:xrNegotiateLoaderApiLayerInterface=_xrNegotiateLoaderApiLayerInterface@12")
endif()

add_executable(vro_test test/test.cpp
    src/engine.cpp src/keyboard.cpp src/wrist.cpp src/config.cpp src/keys.cpp src/panel.cpp src/common.cpp)
target_include_directories(vro_test PRIVATE
    ${CMAKE_SOURCE_DIR}/../third_party/openxr-sdk/include
    ${CMAKE_SOURCE_DIR}/../third_party)
target_compile_definitions(vro_test PRIVATE UNICODE _UNICODE)
target_compile_options(vro_test PRIVATE /EHsc /utf-8)
target_link_libraries(vro_test PRIVATE d3d11 d3d12 gdiplus shell32 ole32 user32)
