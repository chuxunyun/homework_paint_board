// 兼容层：conio.h 的最小替代，让包含 <conio.h> 的示例代码可以编译。
// 注意：这里没有实现 Windows 下的按键无回显读取，仅用于编译通过。
#pragma once

#include <cstdio>

inline int _getch() { return std::getchar(); }
inline int _kbhit() { return 0; }
inline int getch() { return _getch(); }
inline int kbhit() { return _kbhit(); }
