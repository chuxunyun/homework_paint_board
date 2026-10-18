cmake_minimum_required(VERSION 3.20)
project(EasyXPaint LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(paint src/main.cpp)

if(APPLE)
    # ------------------------------------------------------------------
    # macOS：使用自带的 EasyX 兼容层（Cocoa / CoreGraphics），无需第三方库
    # ------------------------------------------------------------------
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 17)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

    target_sources(paint PRIVATE compat/src/easyx_mac.mm)
    target_include_directories(paint PRIVATE compat/include)

    set_source_files_properties(compat/src/easyx_mac.mm PROPERTIES
        COMPILE_OPTIONS "-fobjc-arc;-Wno-deprecated-declarations")

    target_link_libraries(paint PRIVATE
        "-framework Cocoa"
        "-framework CoreText"
        "-framework ImageIO")

    target_compile_options(paint PRIVATE -Wall -Wextra -Wno-unused-parameter)

elseif(WIN32)
    # ------------------------------------------------------------------
    # Windows：官方 EasyX（Visual Studio 或 CLion + MinGW）
    # 把环境变量 EASYX_ROOT 指向 EasyX 安装目录即可，例如：
    #     EASYX_ROOT=C:/EasyX
    # 目录结构应为  include/graphics.h 与 lib64/libeasyxw.a（MinGW）
    # ------------------------------------------------------------------
    if(DEFINED ENV{EASYX_ROOT})
        set(EASYX_ROOT "$ENV{EASYX_ROOT}")
    endif()

    if(EASYX_ROOT)
        target_include_directories(paint PRIVATE "${EASYX_ROOT}/include")
        if(EXISTS "${EASYX_ROOT}/lib64")
            target_link_directories(paint PRIVATE "${EASYX_ROOT}/lib64")
        else()
            target_link_directories(paint PRIVATE "${EASYX_ROOT}/lib")
        endif()
    endif()

    # 统一按 Unicode 编译，保证中文文字正常显示
    target_compile_definitions(paint PRIVATE UNICODE _UNICODE)
    if(MSVC)
        # EasyX 头文件里的 #pragma comment(lib, "EasyXw.lib") 会自动完成链接
        target_compile_options(paint PRIVATE /utf-8)
    elseif(MINGW)
        # Unicode 版本对应的库是 libeasyxw.a
        target_compile_options(paint PRIVATE -finput-charset=UTF-8 -Wno-unknown-pragmas)
        target_link_libraries(paint PRIVATE easyxw gdi32 ole32 uuid imm32 msimg32)
    endif()

else()
    message(FATAL_ERROR "本项目支持 macOS（内置兼容层）与 Windows（官方 EasyX）。")
endif()
