cmake_minimum_required(VERSION 3.20)
project(EasyXPaint LANGUAGES CXX)

if(NOT APPLE)
    message(FATAL_ERROR "本项目仅支持 macOS。")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 兼容层用 Objective-C++ 调用 Cocoa / CoreGraphics
enable_language(OBJCXX)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

add_executable(paint
    src/main.cpp
    compat/src/easyx_mac.mm)

target_include_directories(paint PRIVATE compat/include)

set_source_files_properties(compat/src/easyx_mac.mm PROPERTIES
    COMPILE_OPTIONS "-fobjc-arc;-Wno-deprecated-declarations")

target_link_libraries(paint PRIVATE
    "-framework Cocoa"
    "-framework CoreText"
    "-framework ImageIO")

target_compile_options(paint PRIVATE -Wall -Wextra -Wno-unused-parameter)
