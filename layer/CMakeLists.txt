cmake_minimum_required(VERSION 3.20)
project(OpenXRCompanion CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_library(OpenXRCompanion SHARED
    src/layer.cpp src/engine.cpp src/screen_ui.cpp src/wheel.cpp src/adjust.cpp src/vr_keyboard.cpp src/wrist.cpp src/config.cpp src/keys.cpp src/panel.cpp src/common.cpp src/screen.cpp src/wincap.cpp src/offsets_panel.cpp src/mirror.cpp src/tracking_log.cpp src/head_smoothing.cpp src/game_offsets.cpp src/overlay.cpp src/game_input.cpp)
target_include_directories(OpenXRCompanion PRIVATE
    ${CMAKE_SOURCE_DIR}/../third_party/openxr-sdk/include
    ${CMAKE_SOURCE_DIR}/../third_party)
target_compile_definitions(OpenXRCompanion PRIVATE UNICODE _UNICODE)
target_compile_options(OpenXRCompanion PRIVATE /W3 /EHsc /utf-8)
target_link_libraries(OpenXRCompanion PRIVATE d3dcompiler d3d11 d3d12 dxgi dwmapi windowsapp gdiplus shell32 ole32 user32)
# 32-bit: __stdcall decorates the export (_name@12); the OpenXR loader looks it up undecorated
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_link_options(OpenXRCompanion PRIVATE
        "/EXPORT:xrNegotiateLoaderApiLayerInterface=_xrNegotiateLoaderApiLayerInterface@12")
endif()

add_executable(vro_test test/test.cpp test/test_parts.cpp
    src/engine.cpp src/screen_ui.cpp src/wheel.cpp src/adjust.cpp src/vr_keyboard.cpp src/wrist.cpp src/config.cpp src/keys.cpp src/panel.cpp src/common.cpp src/screen.cpp src/wincap.cpp src/offsets_panel.cpp src/mirror.cpp src/tracking_log.cpp src/head_smoothing.cpp src/game_offsets.cpp src/overlay.cpp src/game_input.cpp)
target_include_directories(vro_test PRIVATE
    ${CMAKE_SOURCE_DIR}/../third_party/openxr-sdk/include
    ${CMAKE_SOURCE_DIR}/../third_party)
target_compile_definitions(vro_test PRIVATE UNICODE _UNICODE)
target_compile_options(vro_test PRIVATE /EHsc /utf-8)
target_link_libraries(vro_test PRIVATE d3dcompiler d3d11 d3d12 dxgi dwmapi windowsapp gdiplus shell32 ole32 user32)
