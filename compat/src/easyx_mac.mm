// ============================================================================
//  easyx_mac.mm —— EasyX 兼容层的 macOS 实现
//  ---------------------------------------------------------------------------
//  用 CoreGraphics 把 EasyX 的绘图函数画进一块 BGRA 像素缓冲区，再显示到
//  Cocoa 窗口里的 NSImageView 上；鼠标/键盘事件转换成 EasyX 的 ExMessage。
// ============================================================================
#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>

#include "graphics.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <vector>
#include <unistd.h>

// ---------------------------------------------------------------------------
// 全局状态
// ---------------------------------------------------------------------------
static CGContextRef          g_ctx = nullptr;
static std::vector<unsigned char> g_pixels;
static int                   g_w = 0;
static int                   g_h = 0;
static float                 g_scale = 2.0f;

static COLORREF g_bk        = RGB(255, 255, 255);
static COLORREF g_line      = RGB(0, 0, 0);
static COLORREF g_fill      = RGB(255, 255, 255);
static COLORREF g_textColor = RGB(0, 0, 0);
static int      g_textH     = 16;
static int      g_bkMode    = OPAQUE;
static int      g_lineWidth = 1;

static bool g_ready     = false;
static bool g_offscreen = false;

static std::deque<ExMessage> g_queue;

static NSWindow*  g_window = nil;
static NSImageView* g_view = nil;
static id         g_delegate = nil;
static bool       g_appReady = false;
static bool       g_windowClosed = false;
static bool       g_mouseDown = false;

static NSString*  g_fontName = @"PingFang SC";

// ---------------------------------------------------------------------------
// 颜色 / 图形环境
// ---------------------------------------------------------------------------
static inline CGFloat chan(COLORREF c, int shift) {
    return (CGFloat)((c >> shift) & 0xFF) / 255.0;
}

static void setStrokeRGB(COLORREF c) {
    CGContextSetRGBStrokeColor(g_ctx, chan(c, 0), chan(c, 8), chan(c, 16), 1.0);
}

static void setFillRGB(COLORREF c) {
    CGContextSetRGBFillColor(g_ctx, chan(c, 0), chan(c, 8), chan(c, 16), 1.0);
}

static void prepareStroke() {
    CGContextSetLineWidth(g_ctx, (CGFloat)std::max(1, g_lineWidth));
    CGContextSetShouldAntialias(g_ctx, g_lineWidth > 1);
    CGContextSetLineCap(g_ctx, kCGLineCapRound);
    CGContextSetLineJoin(g_ctx, kCGLineJoinRound);
    setStrokeRGB(g_line);
}

static void normalizeRect(int& l, int& t, int& r, int& b) {
    if (l > r) std::swap(l, r);
    if (t > b) std::swap(t, b);
}

static void destroySurface() {
    if (g_ctx) {
        CGContextRelease(g_ctx);
        g_ctx = nullptr;
    }
    g_pixels.clear();
}

static bool createSurface(int w, int h, float scale) {
    if (w <= 0 || h <= 0) return false;
    if (scale < 1.0f) scale = 1.0f;

    g_w = w;
    g_h = h;
    g_scale = scale;

    const int pxW = (int)std::lround(w * scale);
    const int pxH = (int)std::lround(h * scale);
    const size_t rowBytes = (size_t)pxW * 4;

    g_pixels.assign(rowBytes * (size_t)pxH, 0);

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    g_ctx = CGBitmapContextCreate(g_pixels.data(), (size_t)pxW, (size_t)pxH, 8, rowBytes,
                                  cs, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(cs);
    if (!g_ctx) {
        g_pixels.clear();
        return false;
    }

    // 坐标系改成本地像素、y 轴向下，与 EasyX 一致
    CGContextTranslateCTM(g_ctx, 0, (CGFloat)pxH);
    CGContextScaleCTM(g_ctx, scale, -scale);
    CGContextSetAllowsAntialiasing(g_ctx, true);
    CGContextSetInterpolationQuality(g_ctx, kCGInterpolationHigh);
    CGContextSetShouldSmoothFonts(g_ctx, true);
    return true;
}

// ---------------------------------------------------------------------------
// 文字
// ---------------------------------------------------------------------------
static NSString* toNSString(LPCTSTR s) {
    if (!s) return @"";
    std::u16string u;
    for (const wchar_t* p = s; *p; ++p) {
        unsigned int c = (unsigned int)(*p);
        if (c < 0x10000) {
            u.push_back((char16_t)c);
        } else {
            c -= 0x10000;
            u.push_back((char16_t)(0xD800 + (c >> 10)));
            u.push_back((char16_t)(0xDC00 + (c & 0x3FF)));
        }
    }
    return [NSString stringWithCharacters:(const unichar*)u.data() length:(NSUInteger)u.size()];
}

static NSFont* fontOfSize(int size) {
    NSFont* f = [NSFont fontWithName:g_fontName size:(CGFloat)size];
    if (!f) f = [NSFont systemFontOfSize:(CGFloat)size];
    return f;
}

static NSAttributedString* attributedText(LPCTSTR s, int size, COLORREF color) {
    NSDictionary* attrs = @{
        NSFontAttributeName : fontOfSize(size),
        NSForegroundColorAttributeName : [NSColor colorWithSRGBRed:chan(color, 0)
                                                             green:chan(color, 8)
                                                              blue:chan(color, 16)
                                                             alpha:1.0]
    };
    return [[NSAttributedString alloc] initWithString:toNSString(s) attributes:attrs];
}

// ---------------------------------------------------------------------------
// 显示
// ---------------------------------------------------------------------------
static void presentNow() {
    if (g_offscreen || !g_ctx || !g_view) return;
    @autoreleasepool {
        CGImageRef image = CGBitmapContextCreateImage(g_ctx);
        if (!image) return;
        NSImage* nsImage = [[NSImage alloc] initWithCGImage:image
                                                       size:NSMakeSize(g_w, g_h)];
        CGImageRelease(image);
        g_view.image = nsImage;
        [g_view setNeedsDisplay:YES];
        [g_view displayIfNeeded];
    }
}

// ---------------------------------------------------------------------------
// 绘图原语
// ---------------------------------------------------------------------------
void cleardevice() {
    if (!g_ctx) return;
    CGContextSetShouldAntialias(g_ctx, false);
    setFillRGB(g_bk);
    CGContextFillRect(g_ctx, CGRectMake(0, 0, g_w, g_h));
}

void line(int x1, int y1, int x2, int y2) {
    if (!g_ctx) return;
    prepareStroke();
    CGContextBeginPath(g_ctx);
    CGContextMoveToPoint(g_ctx, x1, y1);
    CGContextAddLineToPoint(g_ctx, x2, y2);
    CGContextStrokePath(g_ctx);
}

void rectangle(int left, int top, int right, int bottom) {
    if (!g_ctx) return;
    normalizeRect(left, top, right, bottom);
    prepareStroke();
    CGContextStrokeRect(g_ctx, CGRectMake(left, top, right - left, bottom - top));
}

void fillrectangle(int left, int top, int right, int bottom) {
    if (!g_ctx) return;
    normalizeRect(left, top, right, bottom);
    CGContextSetShouldAntialias(g_ctx, false);
    setFillRGB(g_fill);
    CGContextFillRect(g_ctx, CGRectMake(left, top, right - left, bottom - top));
}

void solidrectangle(int left, int top, int right, int bottom) {
    fillrectangle(left, top, right, bottom);
}

static CGPathRef makeRoundRect(int left, int top, int right, int bottom, int ew, int eh) {
    normalizeRect(left, top, right, bottom);
    CGFloat radius = (CGFloat)std::max(0, std::min(ew, eh)) / 2.0;
    CGRect rect = CGRectMake(left, top, right - left, bottom - top);
    return CGPathCreateWithRoundedRect(rect, radius, radius, nullptr);
}

void roundrect(int left, int top, int right, int bottom, int ew, int eh) {
    if (!g_ctx) return;
    CGPathRef path = makeRoundRect(left, top, right, bottom, ew, eh);
    prepareStroke();
    CGContextAddPath(g_ctx, path);
    CGContextStrokePath(g_ctx);
    CGPathRelease(path);
}

void fillroundrect(int left, int top, int right, int bottom, int ew, int eh) {
    if (!g_ctx) return;
    CGPathRef path = makeRoundRect(left, top, right, bottom, ew, eh);
    CGContextSetShouldAntialias(g_ctx, true);
    setFillRGB(g_fill);
    CGContextAddPath(g_ctx, path);
    CGContextFillPath(g_ctx);
    CGPathRelease(path);
}

void ellipse(int left, int top, int right, int bottom) {
    if (!g_ctx) return;
    normalizeRect(left, top, right, bottom);
    prepareStroke();
    CGContextStrokeEllipseInRect(g_ctx,
        CGRectMake(left, top, right - left, bottom - top));
}

void fillellipse(int left, int top, int right, int bottom) {
    if (!g_ctx) return;
    normalizeRect(left, top, right, bottom);
    CGContextSetShouldAntialias(g_ctx, true);
    setFillRGB(g_fill);
    CGContextFillEllipseInRect(g_ctx,
        CGRectMake(left, top, right - left, bottom - top));
}

void circle(int x, int y, int radius) {
    if (!g_ctx || radius <= 0) return;
    prepareStroke();
    CGContextStrokeEllipseInRect(g_ctx,
        CGRectMake(x - radius, y - radius, radius * 2.0, radius * 2.0));
}

void fillcircle(int x, int y, int radius) {
    if (!g_ctx || radius <= 0) return;
    CGContextSetShouldAntialias(g_ctx, true);
    setFillRGB(g_fill);
    CGContextFillEllipseInRect(g_ctx,
        CGRectMake(x - radius, y - radius, radius * 2.0, radius * 2.0));
}

static void buildPolyline(const POINT* pts, int num, bool close) {
    CGContextBeginPath(g_ctx);
    CGContextMoveToPoint(g_ctx, pts[0].x, pts[0].y);
    for (int i = 1; i < num; ++i) CGContextAddLineToPoint(g_ctx, pts[i].x, pts[i].y);
    if (close) CGContextClosePath(g_ctx);
}

void polyline(const POINT* points, int num) {
    if (!g_ctx || !points || num <= 0) return;
    if (num == 1) {
        setFillRGB(g_line);
        CGContextSetShouldAntialias(g_ctx, true);
        double r = std::max(1, g_lineWidth) / 2.0;
        CGContextFillEllipseInRect(g_ctx,
            CGRectMake(points[0].x - r, points[0].y - r, r * 2, r * 2));
        return;
    }
    prepareStroke();
    buildPolyline(points, num, false);
    CGContextStrokePath(g_ctx);
}

void polygon(const POINT* points, int num) {
    if (!g_ctx || !points || num < 2) return;
    prepareStroke();
    buildPolyline(points, num, true);
    CGContextStrokePath(g_ctx);
}

void fillpolygon(const POINT* points, int num) {
    if (!g_ctx || !points || num < 3) return;
    CGContextSetShouldAntialias(g_ctx, true);
    setFillRGB(g_fill);
    buildPolyline(points, num, true);
    CGContextFillPath(g_ctx);
}

void outtextxy(int x, int y, LPCTSTR text) {
    if (!g_ctx || !text) return;
    @autoreleasepool {
        NSAttributedString* as = attributedText(text, g_textH, g_textColor);
        NSSize size = [as size];
        if (g_bkMode == OPAQUE) {
            CGContextSetShouldAntialias(g_ctx, false);
            setFillRGB(g_bk);
            CGContextFillRect(g_ctx, CGRectMake(x, y, size.width, size.height));
        }
        CGContextSaveGState(g_ctx);
        CGContextSetShouldAntialias(g_ctx, true);
        NSGraphicsContext* gc =
            [NSGraphicsContext graphicsContextWithCGContext:g_ctx flipped:YES];
        [NSGraphicsContext saveGraphicsState];
        [NSGraphicsContext setCurrentContext:gc];
        [as drawAtPoint:NSMakePoint(x, y)];
        [NSGraphicsContext restoreGraphicsState];
        CGContextRestoreGState(g_ctx);
    }
}

int textwidth(LPCTSTR text) {
    if (!text) return 0;
    @autoreleasepool {
        NSAttributedString* as = attributedText(text, g_textH, g_textColor);
        return (int)std::lround([as size].width);
    }
}

int textheight(LPCTSTR text) {
    (void)text;
    @autoreleasepool {
        NSAttributedString* as = attributedText(text, g_textH, g_textColor);
        return (int)std::lround([as size].height);
    }
}

// ---------------------------------------------------------------------------
// 属性设置
// ---------------------------------------------------------------------------
void setbkcolor(COLORREF color)   { g_bk = color; }
COLORREF getbkcolor()             { return g_bk; }
void setlinecolor(COLORREF color) { g_line = color; }
COLORREF getlinecolor()           { return g_line; }
void setfillcolor(COLORREF color) { g_fill = color; }
COLORREF getfillcolor()           { return g_fill; }
void settextcolor(COLORREF color) { g_textColor = color; }
void setbkmode(int mode)          { g_bkMode = mode; }

void setlinestyle(int style, int thickness, const DWORD*, DWORD) {
    (void)style;
    g_lineWidth = std::max(1, thickness);
}

void settextstyle(int height, int width, LPCTSTR face) {
    (void)width;
    @autoreleasepool {
        g_textH = std::max(6, height);
        if (face) g_fontName = toNSString(face);
    }
}

int getwidth()  { return g_w; }
int getheight() { return g_h; }

// ---------------------------------------------------------------------------
// 双缓冲（本实现始终绘制在内存位图上，这里只是把画面“提交”到窗口）
// ---------------------------------------------------------------------------
void BeginBatchDraw() {}
void FlushBatchDraw() { presentNow(); }
void EndBatchDraw()   { presentNow(); }

void Sleep(DWORD milliseconds) {
    if (milliseconds == 0) return;
    usleep((useconds_t)milliseconds * 1000);
}

// ---------------------------------------------------------------------------
// Cocoa 窗口
// ---------------------------------------------------------------------------
@interface EasyXWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation EasyXWindowDelegate
- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    g_windowClosed = true;
    return YES;
}
- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    g_windowClosed = true;
}
@end

@interface EasyXView : NSImageView
@property(nonatomic, strong) NSTrackingArea* easyxTracking;
@end

@implementation EasyXView

- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent*)event { (void)event; return YES; }

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (self.easyxTracking) {
        [self removeTrackingArea:self.easyxTracking];
    }
    self.easyxTracking =
        [[NSTrackingArea alloc] initWithRect:NSZeroRect
                                     options:(NSTrackingMouseMoved |
                                              NSTrackingMouseEnteredAndExited |
                                              NSTrackingActiveInKeyWindow |
                                              NSTrackingInVisibleRect)
                                       owner:self
                                    userInfo:nil];
    [self addTrackingArea:self.easyxTracking];
}

- (void)pushMouse:(NSEvent*)event message:(int)message down:(BOOL)down {
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    ExMessage m;
    m.message = (unsigned short)message;
    m.x = (short)std::lround(p.x);
    m.y = (short)std::lround(p.y);
    m.lbutton = down;
    m.mkLButton = down;
    m.mkMButton = false;
    m.mkRButton = false;
    m.ctrl = ([event modifierFlags] & NSEventModifierFlagControl) != 0;
    m.shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
    easyx_compat::push_event(m);
}

- (void)mouseDown:(NSEvent*)event {
    [[self window] makeFirstResponder:self];
    g_mouseDown = true;
    [self pushMouse:event message:WM_LBUTTONDOWN down:YES];
}

- (void)mouseUp:(NSEvent*)event {
    g_mouseDown = false;
    [self pushMouse:event message:WM_LBUTTONUP down:NO];
}

- (void)mouseDragged:(NSEvent*)event {
    [self pushMouse:event message:WM_MOUSEMOVE down:YES];
}

- (void)mouseMoved:(NSEvent*)event {
    [self pushMouse:event message:WM_MOUSEMOVE down:g_mouseDown];
}

- (void)rightMouseDown:(NSEvent*)event {
    [self pushMouse:event message:WM_RBUTTONDOWN down:NO];
}

- (void)rightMouseUp:(NSEvent*)event {
    [self pushMouse:event message:WM_RBUTTONUP down:NO];
}

- (void)keyDown:(NSEvent*)event {
    NSString* chars = [event charactersIgnoringModifiers];
    if ([chars length] == 0) return;
    unichar c = [chars characterAtIndex:0];
    ExMessage m;
    m.message = WM_KEYDOWN;
    m.vkcode = (unsigned char)(c & 0xFF);
    m.ch = (TCHAR)c;
    m.ctrl = (([event modifierFlags] & NSEventModifierFlagControl) != 0) ||
             (([event modifierFlags] & NSEventModifierFlagCommand) != 0);
    easyx_compat::push_event(m);
}

@end

// ---------------------------------------------------------------------------
// 事件循环
// ---------------------------------------------------------------------------
static void ensureApp() {
    if (g_appReady) return;
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSMenu* mainMenu = [[NSMenu alloc] init];
        NSMenuItem* appItem = [[NSMenuItem alloc] init];
        [mainMenu addItem:appItem];
        NSMenu* appMenu = [[NSMenu alloc] init];
        [appMenu addItemWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
        [appItem setSubmenu:appMenu];
        [NSApp setMainMenu:mainMenu];

        [NSApp finishLaunching];
        [NSApp activateIgnoringOtherApps:YES];
        g_appReady = true;
    }
}

static void createWindow(int w, int h, const char* title) {
    @autoreleasepool {
        ensureApp();

        NSRect content = NSMakeRect(0, 0, w, h);
        NSWindowStyleMask mask = NSWindowStyleMaskTitled |
                                 NSWindowStyleMaskClosable |
                                 NSWindowStyleMaskMiniaturizable;
        g_window = [[NSWindow alloc] initWithContentRect:content
                                               styleMask:mask
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
        if (!g_window) return;

        g_delegate = [[EasyXWindowDelegate alloc] init];
        [g_window setDelegate:g_delegate];
        [g_window setReleasedWhenClosed:NO];
        [g_window setTitle:[NSString stringWithUTF8String:title]];

        g_view = [[EasyXView alloc] initWithFrame:content];
        [g_view setImageScaling:NSImageScaleAxesIndependently];
        [g_view setWantsLayer:YES];
        [g_window setContentView:g_view];
        [g_window center];
        [g_window makeFirstResponder:g_view];
        [g_window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        g_windowClosed = false;
    }
}

static void pumpEvents() {
    if (!g_appReady || !g_window) return;
    @autoreleasepool {
        for (;;) {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                untilDate:[NSDate distantPast]
                                                   inMode:NSDefaultRunLoopMode
                                                  dequeue:YES];
            if (!event) break;
            [NSApp sendEvent:event];
        }
        [NSApp updateWindows];
    }
}

bool easyx_window_alive() {
    return !g_windowClosed;
}

bool peekmessage(ExMessage* msg, int filter, bool removemsg) {
    if (!g_ready || g_offscreen || !msg) return false;
    pumpEvents();

    for (auto it = g_queue.begin(); it != g_queue.end(); ++it) {
        bool isKey = (it->message >= WM_KEYDOWN && it->message <= WM_CHAR);
        int kind = isKey ? EX_KEY : EX_MOUSE;
        if (filter < 0 || (filter & kind)) {
            *msg = *it;
            if (removemsg) g_queue.erase(it);
            return true;
        }
    }
    return false;
}

void flushmessage() {
    pumpEvents();
    g_queue.clear();
}

// ---------------------------------------------------------------------------
// 生命周期
// ---------------------------------------------------------------------------
void initgraph(int width, int height, int flag) {
    (void)flag;
    if (g_ready) closegraph();

    float scale = 2.0f;
    NSScreen* screen = [NSScreen mainScreen];
    if (screen) scale = (float)[screen backingScaleFactor];
    if (scale < 1.0f) scale = 1.0f;

    g_offscreen = false;
    if (!createSurface(width, height, scale)) return;
    createWindow(width, height, "EasyX 画板");
    if (!g_window) {
        destroySurface();
        return;
    }
    g_ready = true;
    cleardevice();
    presentNow();
}

namespace easyx_compat {

void initgraph_offscreen(int width, int height, float scale) {
    if (g_ready) closegraph();
    g_offscreen = true;
    if (!createSurface(width, height, scale)) return;
    g_ready = true;
    cleardevice();
}

bool is_offscreen() { return g_offscreen; }

void push_event(const ExMessage& msg) { g_queue.push_back(msg); }

bool save_png(const char* utf8_path) {
    if (!g_ctx || !utf8_path) return false;
    @autoreleasepool {
        CGImageRef image = CGBitmapContextCreateImage(g_ctx);
        if (!image) return false;
        NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithCGImage:image];
        CGImageRelease(image);
        if (!rep) return false;
        NSData* data = [rep representationUsingType:NSBitmapImageFileTypePNG
                                         properties:@{}];
        if (!data) return false;
        NSString* path = [NSString stringWithUTF8String:utf8_path];
        return [data writeToFile:path atomically:YES] == YES;
    }
}

}  // namespace easyx_compat

void closegraph() {
    if (g_window) {
        [g_window setDelegate:nil];
        [g_window close];
        g_window = nil;
    }
    g_view = nil;
    g_delegate = nil;
    destroySurface();
    g_ready = false;
    g_offscreen = false;
    g_queue.clear();
}
