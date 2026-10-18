// ============================================================================
//  简易画板 —— EasyX 课程作业
//  ---------------------------------------------------------------------------
//  * 顶部工具栏：10 种颜色、填充开关、4 档线宽、撤销、清空
//  * 工具：直线 / 矩形 / 椭圆 / 圆 / 画笔 / 橡皮擦
//  * 画图区：白色画布，按住鼠标左键拖动即可绘制
//  * 快捷键：Esc 退出，Ctrl/⌘ + Z 撤销
//
//  macOS + CLion：使用项目自带的 graphics.h 兼容层直接运行；
//  Windows：使用官方 EasyX，编译同一份源码即可。
// ============================================================================

#if defined(_WIN32)
#  define NOMINMAX
#  include <windows.h>
#  include <graphics.h>
#else
#  include <graphics.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 界面尺寸
// ---------------------------------------------------------------------------
namespace ui {
constexpr int WIN_W     = 1080;
constexpr int WIN_H     = 700;
constexpr int BAR_H     = 108;
constexpr int CANVAS_L  = 28;
constexpr int CANVAS_T  = 136;
constexpr int CANVAS_R  = 1052;
constexpr int CANVAS_B  = 672;
constexpr int MARGIN    = 28;
}  // namespace ui

// ---------------------------------------------------------------------------
// 配色
// ---------------------------------------------------------------------------
namespace col {
const COLORREF windowBg     = RGB(243, 245, 249);
const COLORREF bar          = RGB(255, 255, 255);
const COLORREF divider      = RGB(228, 232, 239);
const COLORREF chip         = RGB(241, 244, 249);
const COLORREF chipHover    = RGB(229, 235, 245);
const COLORREF accent       = RGB(47, 102, 246);
const COLORREF text         = RGB(45, 52, 64);
const COLORREF textDim      = RGB(140, 149, 164);
const COLORREF canvas       = RGB(255, 255, 255);
const COLORREF canvasBorder = RGB(212, 219, 230);
const COLORREF canvasHint   = RGB(196, 203, 214);
}  // namespace col

#ifdef _WIN32
static const TCHAR* const kFont = TEXT("Microsoft YaHei");
#else
static const TCHAR* const kFont = TEXT("PingFang SC");
#endif

// ---------------------------------------------------------------------------
// 调色板
// ---------------------------------------------------------------------------
struct Swatch {
    COLORREF color;
    const TCHAR* name;
};

static const Swatch kPalette[10] = {
    {RGB(28, 28, 30),    TEXT("黑")},
    {RGB(120, 128, 140), TEXT("灰")},
    {RGB(229, 72, 77),   TEXT("红")},
    {RGB(245, 124, 0),   TEXT("橙")},
    {RGB(250, 204, 21),  TEXT("黄")},
    {RGB(34, 164, 93),   TEXT("绿")},
    {RGB(6, 182, 212),   TEXT("青")},
    {RGB(59, 110, 245),  TEXT("蓝")},
    {RGB(139, 92, 246),  TEXT("紫")},
    {RGB(236, 72, 153),  TEXT("粉")},
};

static const int kWidths[4] = {1, 3, 6, 10};

// ---------------------------------------------------------------------------
// 工具
// ---------------------------------------------------------------------------
enum Tool {
    TOOL_LINE = 0,
    TOOL_RECT,
    TOOL_ELLIPSE,
    TOOL_CIRCLE,
    TOOL_PEN,
    TOOL_ERASER,
    TOOL_COUNT
};

static const TCHAR* const kToolNames[TOOL_COUNT] = {
    TEXT("直线"), TEXT("矩形"), TEXT("椭圆"),
    TEXT("圆"),   TEXT("画笔"), TEXT("橡皮擦"),
};

// ---------------------------------------------------------------------------
// 基础几何
// ---------------------------------------------------------------------------
struct Rect {
    int x = 0, y = 0, w = 0, h = 0;

    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
    int cx() const { return x + w / 2; }
    int cy() const { return y + h / 2; }
};

// 一条笔画（图形 / 手绘 / 擦除）
struct Stroke {
    Tool tool = TOOL_LINE;
    POINT a{0, 0};
    POINT b{0, 0};
    COLORREF color = RGB(0, 0, 0);
    int width = 1;
    bool filled = false;
    std::vector<POINT> points;   // 画笔 / 橡皮擦的轨迹
};

// ---------------------------------------------------------------------------
// 画板
// ---------------------------------------------------------------------------
class PaintBoard {
public:
    PaintBoard() { layout(); }

    void init() {
        initgraph(ui::WIN_W, ui::WIN_H);
        setbkcolor(col::windowBg);
        cleardevice();
        BeginBatchDraw();
        render();
    }

#ifndef _WIN32
    // 仅 macOS 调试用：不开窗口，直接渲染一帧
    void initOffscreen() {
        easyx_compat::initgraph_offscreen(ui::WIN_W, ui::WIN_H, 2.0f);
        setbkcolor(col::windowBg);
        cleardevice();
        render();
    }
#endif

    void run() {
        ExMessage msg;
        while (!quit_ && gfxWindowAlive()) {
            if (peekmessage(&msg, EX_MOUSE | EX_KEY)) {
                handleMessage(msg);
            } else {
                Sleep(6);
            }
        }
    }

    bool shouldQuit() const { return quit_; }

    // 处理一条消息（公开，便于无窗口模式下做自动化测试）
    void handleMessage(const ExMessage& msg) {
        switch (msg.message) {
            case WM_LBUTTONDOWN:
                onMouseDown(msg.x, msg.y);
                render();
                break;

            case WM_MOUSEMOVE:
                if (dragging_) {
                    onMouseMove(msg.x, msg.y);
                    render();
                } else {
                    int hover = hitTest(msg.x, msg.y);
                    if (hover != hover_) {
                        hover_ = hover;
                        render();
                    }
                }
                break;

            case WM_LBUTTONUP:
                onMouseUp(msg.x, msg.y);
                render();
                break;

            case WM_KEYDOWN:
                onKeyDown(msg);
                break;

            default:
                break;
        }
    }

private:
    // ----------------------------- 布局 -----------------------------
    Rect swatchRect_[10];
    Rect fillRect_;
    Rect widthRect_[4];
    Rect toolRect_[TOOL_COUNT];
    Rect undoRect_;
    Rect clearRect_;

    void layout() {
        for (int i = 0; i < 10; ++i) {
            swatchRect_[i] = Rect{28 + i * 40, 16, 32, 32};
        }
        fillRect_ = Rect{458, 16, 80, 32};

        for (int i = 0; i < 4; ++i) {
            widthRect_[i] = Rect{622 + i * 40, 16, 32, 32};
        }
        for (int i = 0; i < TOOL_COUNT; ++i) {
            toolRect_[i] = Rect{28 + i * 86, 60, 82, 34};
        }
        undoRect_  = Rect{578, 60, 66, 34};
        clearRect_ = Rect{652, 60, 66, 34};
    }

    // ----------------------------- 状态 -----------------------------
    std::vector<Stroke> strokes_;
    Stroke current_;
    bool dragging_ = false;
    bool quit_ = false;

    Tool tool_ = TOOL_LINE;
    Tool lastShape_ = TOOL_LINE;
    COLORREF penColor_ = kPalette[2].color;
    int lineWidth_ = 3;
    bool fillOn_ = false;
    int hover_ = -1;
    int currentMargin_ = 2;

    static bool gfxWindowAlive() {
#if defined(_WIN32)
        HWND hwnd = GetHWnd();
        return hwnd != nullptr && IsWindow(hwnd) != 0;
#else
        return easyx_window_alive();
#endif
    }

    bool inCanvas(int x, int y) const {
        return x >= ui::CANVAS_L && x <= ui::CANVAS_R &&
               y >= ui::CANVAS_T && y <= ui::CANVAS_B;
    }

    // 笔画有一定粗细，越靠近画布边缘就越可能画到画布之外，
    // 因此按线宽的一半留出安全边距。
    static int strokeMargin(Tool t, int width, bool filled) {
        if ((t == TOOL_RECT || t == TOOL_ELLIPSE || t == TOOL_CIRCLE) && filled) {
            return 2;
        }
        return width / 2 + 2;
    }

    POINT clampToCanvas(int x, int y, int margin) const {
        POINT p;
        p.x = std::max(ui::CANVAS_L + margin, std::min(ui::CANVAS_R - margin, x));
        p.y = std::max(ui::CANVAS_T + margin, std::min(ui::CANVAS_B - margin, y));
        return p;
    }

    // ----------------------------- 命中测试 -----------------------------
    // 0-9 颜色，10 填充，11-14 线宽，20-25 工具，30 撤销，31 清空
    int hitTest(int x, int y) const {
        for (int i = 0; i < 10; ++i) {
            if (swatchRect_[i].contains(x, y)) return i;
        }
        if (fillRect_.contains(x, y)) return 10;
        for (int i = 0; i < 4; ++i) {
            if (widthRect_[i].contains(x, y)) return 11 + i;
        }
        for (int i = 0; i < TOOL_COUNT; ++i) {
            if (toolRect_[i].contains(x, y)) return 20 + i;
        }
        if (undoRect_.contains(x, y)) return 30;
        if (clearRect_.contains(x, y)) return 31;
        return -1;
    }

    // ----------------------------- 鼠标 -----------------------------
    void onMouseDown(int x, int y) {
        if (y < ui::BAR_H) {
            hover_ = hitTest(x, y);
            clickBar(hover_);
            return;
        }
        if (!inCanvas(x, y)) return;

        dragging_ = true;
        hover_ = -1;
        current_ = Stroke{};
        current_.tool = tool_;
        current_.color = (tool_ == TOOL_ERASER) ? col::canvas : penColor_;
        current_.width = (tool_ == TOOL_ERASER) ? (6 + lineWidth_ * 2) : lineWidth_;
        current_.filled = fillOn_ && (tool_ == TOOL_RECT ||
                                      tool_ == TOOL_ELLIPSE ||
                                      tool_ == TOOL_CIRCLE);
        currentMargin_ = strokeMargin(current_.tool, current_.width, current_.filled);
        current_.a = clampToCanvas(x, y, currentMargin_);
        current_.b = current_.a;
        if (tool_ == TOOL_PEN || tool_ == TOOL_ERASER) {
            current_.points.push_back(current_.a);
        }
    }

    void onMouseMove(int x, int y) {
        if (!dragging_) return;
        POINT p = clampToCanvas(x, y, currentMargin_);
        if (current_.tool == TOOL_PEN || current_.tool == TOOL_ERASER) {
            const POINT& last = current_.points.back();
            if (std::abs(p.x - last.x) + std::abs(p.y - last.y) >= 2) {
                current_.points.push_back(p);
            }
        } else {
            current_.b = p;
        }
    }

    void onMouseUp(int x, int y) {
        if (!dragging_) return;
        dragging_ = false;

        POINT p = clampToCanvas(x, y, currentMargin_);
        if (current_.tool == TOOL_PEN || current_.tool == TOOL_ERASER) {
            if (current_.points.empty()) current_.points.push_back(current_.a);
            current_.points.push_back(p);
            strokes_.push_back(current_);
        } else {
            current_.b = p;
            if (std::abs(current_.a.x - current_.b.x) >= 1 ||
                std::abs(current_.a.y - current_.b.y) >= 1) {
                strokes_.push_back(current_);
            }
        }
    }

    void clickBar(int id) {
        if (id >= 0 && id <= 9) {
            penColor_ = kPalette[id].color;
            if (tool_ == TOOL_ERASER) tool_ = lastShape_;
        } else if (id == 10) {
            fillOn_ = !fillOn_;
        } else if (id >= 11 && id <= 14) {
            lineWidth_ = kWidths[id - 11];
        } else if (id >= 20 && id < 20 + TOOL_COUNT) {
            tool_ = static_cast<Tool>(id - 20);
            if (tool_ != TOOL_ERASER) lastShape_ = tool_;
        } else if (id == 30) {
            if (!strokes_.empty()) strokes_.pop_back();
        } else if (id == 31) {
            strokes_.clear();
        }
    }

    void onKeyDown(const ExMessage& msg) {
        const int vk = msg.vkcode;
        if (vk == VK_ESCAPE) {
            quit_ = true;
            return;
        }

#if defined(_WIN32)
        // EasyX 的按键消息里 ctrl 字段与 vkcode 共用同一块内存，
        // 不能直接读，这里用系统函数判断 Ctrl 是否按下。
        const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
#else
        const bool ctrl = msg.ctrl;
#endif

        if (ctrl && (vk == 'z' || vk == 'Z')) {
            if (!strokes_.empty()) {
                strokes_.pop_back();
                render();
            }
        }
    }

    // ----------------------------- 绘制 -----------------------------
    void render() {
        setbkcolor(col::windowBg);
        cleardevice();

        drawBar();
        drawCanvas();

        for (const Stroke& s : strokes_) drawStroke(s);
        if (dragging_) drawStroke(current_);

        FlushBatchDraw();
    }

    static void textAt(int x, int y, const TCHAR* s, int size, COLORREF color) {
        settextstyle(size, 0, kFont);
        settextcolor(color);
        setbkmode(TRANSPARENT);
        outtextxy(x, y, s);
    }

    static int textMeasure(const TCHAR* s, int size) {
        settextstyle(size, 0, kFont);
        return textwidth(s);
    }

    static void textCenter(const Rect& r, const TCHAR* s, int size, COLORREF color) {
        settextstyle(size, 0, kFont);
        int tw = textwidth(s);
        int th = textheight(s);
        settextcolor(color);
        setbkmode(TRANSPARENT);
        outtextxy(r.x + (r.w - tw) / 2, r.y + (r.h - th) / 2, s);
    }

    // EasyX 的 fill* 系列会同时用“当前线条颜色”画一圈边框。
    // 想得到无边框的实心效果，就把线条颜色设成填充色、线宽设为 1。
    static void beginFill(COLORREF c) {
        setlinestyle(PS_SOLID, 1);
        setlinecolor(c);
        setfillcolor(c);
    }

    static void chipBase(const Rect& r, bool active, bool hot) {
        beginFill(active ? col::accent : (hot ? col::chipHover : col::chip));
        fillroundrect(r.x, r.y, r.x + r.w, r.y + r.h, 12, 12);
    }

    void drawBar() {
        beginFill(col::bar);
        fillrectangle(0, 0, ui::WIN_W, ui::BAR_H);

        setlinestyle(PS_SOLID, 1);
        setlinecolor(col::divider);
        line(0, ui::BAR_H - 1, ui::WIN_W, ui::BAR_H - 1);

        // ---- 第一排：颜色 / 填充 / 线宽 / 提示 ----
        for (int i = 0; i < 10; ++i) drawSwatch(i);

        chipBase(fillRect_, fillOn_, hover_ == 10);
        textCenter(fillRect_, TEXT("填充"), 14,
                   fillOn_ ? RGB(255, 255, 255) : col::text);

        textAt(566, 25, TEXT("线宽"), 13, col::textDim);
        for (int i = 0; i < 4; ++i) drawWidthChip(i);

#ifdef _WIN32
        const TCHAR* hint = TEXT("Esc 退出  ·  Ctrl+Z 撤销");
#else
        const TCHAR* hint = TEXT("Esc 退出  ·  Cmd+Z 撤销");
#endif
        int hintW = textMeasure(hint, 12);
        textAt(ui::WIN_W - ui::MARGIN - hintW, 26, hint, 12, col::textDim);

        // ---- 第二排：工具 / 撤销 / 清空 / 当前工具 ----
        for (int i = 0; i < TOOL_COUNT; ++i) {
            chipBase(toolRect_[i], tool_ == i, hover_ == 20 + i);
            textCenter(toolRect_[i], kToolNames[i], 14,
                       tool_ == i ? RGB(255, 255, 255) : col::text);
        }

        setlinestyle(PS_SOLID, 1);
        setlinecolor(col::divider);
        line(562, 64, 562, 90);

        chipBase(undoRect_, false, hover_ == 30);
        textCenter(undoRect_, TEXT("撤销"), 14,
                   strokes_.empty() ? col::textDim : col::text);

        chipBase(clearRect_, false, hover_ == 31);
        textCenter(clearRect_, TEXT("清空"), 14,
                   strokes_.empty() ? col::textDim : col::text);

        const TCHAR* prefix = TEXT("当前工具：");
        const TCHAR* name = kToolNames[tool_];
        int w = textMeasure(prefix, 13) + textMeasure(name, 13);
        int x = ui::WIN_W - ui::MARGIN - w;
        textAt(x, 69, prefix, 13, col::textDim);
        textAt(x + textMeasure(prefix, 13), 69, name, 13, col::accent);
    }

    void drawSwatch(int i) {
        const Rect& r = swatchRect_[i];
        bool selected = kPalette[i].color == penColor_;
        bool hot = hover_ == i;

        if (selected) {
            beginFill(col::accent);
            fillroundrect(r.x - 3, r.y - 3, r.x + r.w + 3, r.y + r.h + 3, 12, 12);
        } else if (hot) {
            beginFill(col::chipHover);
            fillroundrect(r.x - 3, r.y - 3, r.x + r.w + 3, r.y + r.h + 3, 12, 12);
        }

        beginFill(kPalette[i].color);
        fillroundrect(r.x, r.y, r.x + r.w, r.y + r.h, 8, 8);
    }

    void drawWidthChip(int i) {
        const Rect& r = widthRect_[i];
        bool selected = kWidths[i] == lineWidth_;
        chipBase(r, selected, hover_ == 11 + i);

        setlinestyle(PS_SOLID, kWidths[i]);
        setlinecolor(selected ? RGB(255, 255, 255) : col::text);
        line(r.x + 8, r.cy(), r.x + r.w - 8, r.cy());
    }

    void drawCanvas() {
        beginFill(col::canvas);
        fillrectangle(ui::CANVAS_L, ui::CANVAS_T, ui::CANVAS_R, ui::CANVAS_B);

        setlinestyle(PS_SOLID, 1);
        setlinecolor(col::canvasBorder);
        rectangle(ui::CANVAS_L, ui::CANVAS_T, ui::CANVAS_R - 1, ui::CANVAS_B - 1);

        if (strokes_.empty() && !dragging_) {
            const TCHAR* hint = TEXT("按住鼠标左键拖动，开始绘制");
            int w = textMeasure(hint, 16);
            int x = ui::CANVAS_L + (ui::CANVAS_R - ui::CANVAS_L - w) / 2;
            int y = ui::CANVAS_T + (ui::CANVAS_B - ui::CANVAS_T) / 2 - 10;
            textAt(x, y, hint, 16, col::canvasHint);
        }
    }

    void drawStroke(const Stroke& s) {
        switch (s.tool) {
            case TOOL_LINE:
                setlinestyle(PS_SOLID, s.width);
                setlinecolor(s.color);
                line(s.a.x, s.a.y, s.b.x, s.b.y);
                break;

            case TOOL_RECT: {
                int l = std::min(s.a.x, s.b.x);
                int t = std::min(s.a.y, s.b.y);
                int r = std::max(s.a.x, s.b.x);
                int b = std::max(s.a.y, s.b.y);
                if (s.filled) {
                    beginFill(s.color);
                    fillrectangle(l, t, r, b);
                } else {
                    setlinestyle(PS_SOLID, s.width);
                    setlinecolor(s.color);
                    rectangle(l, t, r, b);
                }
                break;
            }

            case TOOL_ELLIPSE: {
                int l = std::min(s.a.x, s.b.x);
                int t = std::min(s.a.y, s.b.y);
                int r = std::max(s.a.x, s.b.x);
                int b = std::max(s.a.y, s.b.y);
                if (s.filled) {
                    beginFill(s.color);
                    fillellipse(l, t, r, b);
                } else {
                    setlinestyle(PS_SOLID, s.width);
                    setlinecolor(s.color);
                    ellipse(l, t, r, b);
                }
                break;
            }

            case TOOL_CIRCLE: {
                int dx = s.b.x - s.a.x;
                int dy = s.b.y - s.a.y;
                int radius = (int)std::lround(std::sqrt((double)dx * dx + (double)dy * dy));
                // 限制半径，保证圆不会画到画布外面
                int limit = std::min(std::min(s.a.x - ui::CANVAS_L, ui::CANVAS_R - s.a.x),
                                     std::min(s.a.y - ui::CANVAS_T, ui::CANVAS_B - s.a.y));
                limit -= s.filled ? 1 : (s.width / 2 + 1);
                if (limit < 1) limit = 1;
                radius = std::max(1, std::min(radius, limit));
                if (s.filled) {
                    beginFill(s.color);
                    fillcircle(s.a.x, s.a.y, radius);
                } else {
                    setlinestyle(PS_SOLID, s.width);
                    setlinecolor(s.color);
                    circle(s.a.x, s.a.y, radius);
                }
                break;
            }

            case TOOL_PEN:
            case TOOL_ERASER: {
                setlinestyle(PS_SOLID, s.width);
                setlinecolor(s.color);
                if (s.points.size() > 1) {
                    polyline(s.points.data(), (int)s.points.size());
                } else if (s.points.size() == 1) {
                    beginFill(s.color);
                    fillcircle(s.points[0].x, s.points[0].y, std::max(1, s.width / 2));
                }
                break;
            }

            default:
                break;
        }
    }
};

// ---------------------------------------------------------------------------
// 无窗口截图（仅 macOS 调试用）
// ---------------------------------------------------------------------------
#ifndef _WIN32
namespace {

void sendMouse(PaintBoard& board, int message, int x, int y) {
    ExMessage m;
    m.message = (unsigned short)message;
    m.x = (short)x;
    m.y = (short)y;
    m.lbutton = (message == WM_LBUTTONDOWN);
    board.handleMessage(m);
}

void clickAt(PaintBoard& board, int x, int y) {
    sendMouse(board, WM_LBUTTONDOWN, x, y);
    sendMouse(board, WM_LBUTTONUP, x, y);
}

void dragTo(PaintBoard& board, int x1, int y1, int x2, int y2, int steps = 20) {
    sendMouse(board, WM_LBUTTONDOWN, x1, y1);
    for (int i = 1; i <= steps; ++i) {
        int x = x1 + (x2 - x1) * i / steps;
        int y = y1 + (y2 - y1) * i / steps;
        sendMouse(board, WM_MOUSEMOVE, x, y);
    }
    sendMouse(board, WM_LBUTTONUP, x2, y2);
}

void clickTool(PaintBoard& board, int tool) {
    clickAt(board, 28 + tool * 86 + 41, 77);
}

void clickSwatch(PaintBoard& board, int index) {
    clickAt(board, 28 + index * 40 + 16, 32);
}

void clickWidth(PaintBoard& board, int index) {
    clickAt(board, 622 + index * 40 + 16, 32);
}

int renderPreview(const char* path) {
    PaintBoard board;
    board.initOffscreen();

    // 矩形
    clickSwatch(board, 7);
    clickTool(board, TOOL_RECT);
    dragTo(board, 110, 196, 330, 360);

    // 椭圆
    clickSwatch(board, 2);
    clickTool(board, TOOL_ELLIPSE);
    dragTo(board, 372, 200, 556, 352);

    // 粗直线
    clickSwatch(board, 5);
    clickWidth(board, 3);
    clickTool(board, TOOL_LINE);
    dragTo(board, 600, 196, 792, 352);

    // 画笔涂鸦
    clickSwatch(board, 8);
    clickTool(board, TOOL_PEN);
    sendMouse(board, WM_LBUTTONDOWN, 832, 250);
    for (int i = 1; i <= 60; ++i) {
        int x = 832 + i * 3;
        int y = 250 + (int)(70 * std::sin(i * 0.22));
        sendMouse(board, WM_MOUSEMOVE, x, y);
    }
    sendMouse(board, WM_LBUTTONUP, 832 + 180, 250);

    // 实心圆
    clickSwatch(board, 3);
    clickAt(board, 458 + 40, 32);   // 打开“填充”
    clickTool(board, TOOL_CIRCLE);
    dragTo(board, 190, 548, 268, 548, 10);

    // 实心椭圆
    clickSwatch(board, 6);
    clickTool(board, TOOL_ELLIPSE);
    dragTo(board, 380, 452, 556, 632);

    // 橡皮擦擦过矩形
    clickTool(board, TOOL_ERASER);
    dragTo(board, 122, 268, 338, 268, 24);

    // 结束状态：选中画笔
    clickTool(board, TOOL_PEN);

    if (!easyx_compat::save_png(path)) return 1;
    return 0;
}

}  // namespace
#endif

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
#ifndef _WIN32
    if (argc >= 3 && std::string(argv[1]) == "--shot") {
        return renderPreview(argv[2]);
    }
#endif
    (void)argc;
    (void)argv;

    PaintBoard board;
    board.init();
    board.run();
    closegraph();
    return 0;
}
