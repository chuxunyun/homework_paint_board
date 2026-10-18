// 兼容层：让 "#include <windows.h>" 的示例代码在 macOS 上也能编译。
// 真正需要 Windows API 时才包含这个文件；本项目的 main.cpp 不依赖它。
#pragma once

#include "graphics.h"
