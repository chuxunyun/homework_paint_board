// ============================================================================
//  graphics.h —— EasyX 兼容层（macOS 版）
// ----------------------------------------------------------------------------
//  在 macOS 上提供与 EasyX 同名、同参数的接口，内部使用 Cocoa/CoreGraphics
//  实现。这样，按 EasyX 写法编写的画板源码可以在 Mac + CLion 上直接编译、运行。
//
//  支持的常用接口：
//      initgraph / closegraph / cleardevice / getwidth / getheight
//      颜色：setbkcolor / setlinecolor / setfillcolor / settextcolor
//      线型：setlinestyle(PS_SOLID, 粗细)
//      图形：line / rectangle / fillrectangle / roundrect / fillroundrect
//            ellipse / fillellipse / circle / fillcircle / polyline / polygon
//      文字：settextstyle / outtextxy / textwidth / textheight / setbkmode
//      消息：peekmessage / ExMessage / 鼠标消息 / 键盘消息
//      其他：BeginBatchDraw / FlushBatchDraw / EndBatchDraw / Sleep
//
//  限制：IMAGE / 图片读写 / 路径与区域裁剪 / 旋转文字等高级接口未实现。
// ============================================================================
#pragma once
#ifndef EASYX_COMPAT_GRAPHICS_H
#define EASYX_COMPAT_GRAPHICS_H

#include <cstddef>

// ---------------------------------------------------------------------------
// EasyX 的基础类型
// ---------------------------------------------------------------------------
typedef unsigned int  COLORREF;   // 0x00BBGGRR，与 EasyX 一致
typedef unsigned int  DWORD;
typedef wchar_t       TCHAR;
typedef TCHAR*        LPTSTR;
typedef const TCHAR*  LPCTSTR;

struct tagPOINT { int x; int y; };
typedef tagPOINT POINT;
typedef POINT* LPPOINT;

#ifndef RGB
#define RGB(r, g, b)                                                     \
    ((COLORREF)(((unsigned int)(unsigned char)(r)) |                     \
                (((unsigned int)(unsigned char)(g)) << 8) |              \
                (((unsigned int)(unsigned char)(b)) << 16)))
#endif

// 常用颜色常量
#ifndef BLACK
#define BLACK      RGB(0, 0, 0)
#define WHITE      RGB(255, 255, 255)
#define RED        RGB(255, 0, 0)
#define GREEN      RGB(0, 255, 0)
#define BLUE       RGB(0, 0, 255)
#define YELLOW     RGB(255, 255, 0)
#define CYAN       RGB(0, 255, 255)
#define MAGENTA    RGB(255, 0, 255)
#define GRAY       RGB(128, 128, 128)
#define LIGHTGRAY  RGB(211, 211, 211)
#define DARKGRAY   RGB(169, 169, 169)
#endif

#ifndef TEXT
#define TEXT(x) L##x
#endif
#ifndef _T
#define _T(x) TEXT(x)
#endif

// ---------------------------------------------------------------------------
// 常量
// ---------------------------------------------------------------------------
constexpr unsigned short WM_KEYDOWN    = 0x0100;
constexpr unsigned short WM_KEYUP      = 0x0101;
constexpr unsigned short WM_CHAR       = 0x0102;
constexpr unsigned short WM_MOUSEMOVE   = 0x0200;
constexpr unsigned short WM_LBUTTONDOWN = 0x0201;
constexpr unsigned short WM_LBUTTONUP   = 0x0202;
constexpr unsigned short WM_RBUTTONDOWN = 0x0204;
constexpr unsigned short WM_RBUTTONUP   = 0x0205;
constexpr unsigned short WM_MOUSEWHEEL  = 0x020A;

constexpr int EX_MOUSE  = 1;
constexpr int EX_KEY    = 2;
constexpr int EX_CHAR   = 4;
constexpr int EX_WINDOW = 8;

constexpr int PS_SOLID = 0;

constexpr int TRANSPARENT = 1;
constexpr int OPAQUE      = 2;

constexpr int VK_ESCAPE = 0x1B;

// ---------------------------------------------------------------------------
// 消息结构体（字段名与 EasyX 保持一致）
// ---------------------------------------------------------------------------
struct ExMessage {
    unsigned short message = 0;
    bool ctrl    = false;
    bool shift   = false;
    bool lbutton = false;
    bool mbutton = false;
    bool rbutton = false;
    short x = 0;
    short y = 0;
    short wheel = 0;
    unsigned char vkcode = 0;
    unsigned char scancode = 0;
    bool extended = false;
    bool prevdown = false;
    TCHAR ch = 0;
};

// ---------------------------------------------------------------------------
// 接口
// ---------------------------------------------------------------------------
void initgraph(int width, int height, int flag = 0);
void closegraph();

int  getwidth();
int  getheight();

void cleardevice();
void setbkcolor(COLORREF color);
COLORREF getbkcolor();
void setlinecolor(COLORREF color);
COLORREF getlinecolor();
void setfillcolor(COLORREF color);
COLORREF getfillcolor();
void settextcolor(COLORREF color);

void setbkmode(int mode);
void setlinestyle(int style, int thickness = 1,
                  const DWORD* puserstyle = nullptr, DWORD userstylecount = 0);
void settextstyle(int height, int width, LPCTSTR face);

void line(int x1, int y1, int x2, int y2);
void rectangle(int left, int top, int right, int bottom);
void fillrectangle(int left, int top, int right, int bottom);
void solidrectangle(int left, int top, int right, int bottom);
void roundrect(int left, int top, int right, int bottom,
               int ellipsewidth, int ellipseheight);
void fillroundrect(int left, int top, int right, int bottom,
                   int ellipsewidth, int ellipseheight);
void ellipse(int left, int top, int right, int bottom);
void fillellipse(int left, int top, int right, int bottom);
void circle(int x, int y, int radius);
void fillcircle(int x, int y, int radius);
void polyline(const POINT* points, int num);
void polygon(const POINT* points, int num);
void fillpolygon(const POINT* points, int num);

void outtextxy(int x, int y, LPCTSTR text);
int  textwidth(LPCTSTR text);
int  textheight(LPCTSTR text);

bool peekmessage(ExMessage* msg, int filter = -1, bool removemsg = true);
void flushmessage();

void BeginBatchDraw();
void FlushBatchDraw();
void EndBatchDraw();

void Sleep(DWORD milliseconds);

// ---------------------------------------------------------------------------
// 兼容层专用（仅 macOS 编译时存在）
// ---------------------------------------------------------------------------
bool easyx_window_alive();

namespace easyx_compat {

// 无窗口渲染：用于在 CI / 命令行生成界面截图
void initgraph_offscreen(int width, int height, float scale = 2.0f);
bool is_offscreen();
bool save_png(const char* utf8_path);
void push_event(const ExMessage& msg);

}  // namespace easyx_compat

#endif  // EASYX_COMPAT_GRAPHICS_H
