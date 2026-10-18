# EasyX 简易画板（macOS + CLion 可直接运行）

一份源码，两种环境：

* **Mac + CLion**：用项目自带的 EasyX 兼容层直接编译、运行、调试；
* **Windows + 官方 EasyX**：同一个 `src/main.cpp` 原样编译，交给老师验收。

![界面预览](docs/preview.png)

---

## 一、为什么需要这个项目

官方 EasyX 是基于 **Windows GDI** 的图形库，安装包只提供 MSVC / MinGW 版本；
`graphics.h` 里用到的 `windows.h`、`COLORREF`、`ExMessage` 等也都是 Windows 专有的。
所以 macOS 上没法安装 EasyX，也没法直接用 EasyX 的头文件和库。

这个项目在 `compat/` 目录里实现了一个 **同名、同参数** 的 EasyX 兼容层：

* 头文件仍然叫 `graphics.h`，函数仍然是 `initgraph / line / rectangle / outtextxy ...`
* 内部用 macOS 自带的 Cocoa + CoreGraphics 画图、开窗口、收鼠标键盘消息；
* 不依赖 SDL、Qt 等任何第三方库，CLion 打开就能编译。

因此 `src/main.cpp` 里写的是**标准的 EasyX 代码**，没有大段 `#ifdef` 分支；
在 Windows 上把 `compat/` 换成官方 EasyX 即可。

---

## 二、功能

| 功能 | 说明 |
| --- | --- |
| 颜色选择 | 黑、灰、红、橙、黄、绿、青、蓝、紫、粉，共 10 种 |
| 形状选择 | 直线、矩形、椭圆、圆 |
| 画笔 | 自由手绘，线条平滑 |
| 橡皮擦 | 按住左键擦除，粗细随线宽变化 |
| 画图区 | 独立白色画布，笔画不会画到画布外 |
| 填充 | 矩形 / 椭圆 / 圆可以切换实心或空心 |
| 线宽 | 1 / 3 / 6 / 10 四档 |
| 实时预览 | 拖动过程中就能看到形状，松开鼠标才落笔 |
| 撤销 / 清空 | 工具栏按钮，或 Ctrl+Z / Cmd+Z |
| 状态提示 | 右上角显示当前工具，空画布有操作提示 |

快捷键：`Esc` 退出，`Ctrl+Z`（Windows）/ `Cmd+Z`（macOS）撤销。

---

## 三、用 CLion 运行

1. 打开 CLion，选择 **File → Open**，打开本项目文件夹 `homework`。
2. 如果弹出 “Trust project”，选择 **Trust Project**。
3. 等待右下角 CMake 配置完成（第一次大约十几秒）。
   如果没自动开始：右键 `CMakeLists.txt` → **Reload CMake Project**。
4. 右上角运行配置选择 **paint**，点击绿色三角运行（或 `Ctrl+R`）。
5. 弹出 1080×700 的画板窗口即可开始画图。

> 如果运行配置里没有 `paint`：
> **Run → Edit Configurations → + → CMake Application**，
> Target 选择 `paint`，即可运行。

命令行方式（可选）：

```bash
cmake -S . -B build
cmake --build build -j
./build/paint
```

环境要求：macOS（Intel / Apple Silicon 均可）、Xcode Command Line Tools
（终端执行 `xcode-select --install` 安装）、CMake 3.20+。
CLion 自带的 CMake 就够用。

---

## 四、操作说明

1. 在顶部第一排点一个颜色；
2. 在第二排点一个工具（直线 / 矩形 / 椭圆 / 圆 / 画笔 / 橡皮擦）；
3. 在下方白色画布上**按住鼠标左键拖动**：
   * 直线、矩形、椭圆：从起点拖到终点；
   * 圆：按下的位置是圆心，拖动距离是半径；
   * 画笔、橡皮擦：按住拖动即可连续绘制 / 擦除；
4. 松开左键完成绘制；
5. 画错了点“撤销”，想重来点“清空”。

“填充”按钮控制矩形 / 椭圆 / 圆是实心还是空心；
线宽按钮同时决定画笔的粗细，橡皮擦的粗细会跟着线宽变化。

---

## 五、交给老师（Windows + 官方 EasyX）

`src/main.cpp` 只使用了 EasyX 的标准接口，**不需要任何修改**。
推荐用下面的方式在 Windows 上编译：

### 方式 A：Visual Studio + EasyX（最稳妥）

1. 安装 Visual Studio（Community 版即可）和 EasyX 官方安装包；
2. 新建一个 **空 C++ 项目**；
3. 把 `src/main.cpp` 添加进项目；
4. 项目属性 → C/C++ → 语言 → 把“符合模式”设为“否”，
   并在命令行参数里加上 `/utf-8`（避免中文乱码）；
5. 生成 x64 程序并运行。

### 方式 B：CLion + MinGW + EasyX

1. 下载 EasyX 的 MinGW 版本，解压到某个目录，例如 `C:/EasyX`（里面有 `include/`、`lib64/`）；
2. 设置环境变量 `EASYX_ROOT=C:/EasyX`；
3. 用 CLion 打开本项目，CMake 会自动加上头文件和库路径；
4. 运行 target `paint`。

> 说明：`compat/` 只是 macOS 开发时用的兼容层，Windows 编译时不会参与；
> 如果老师只收源码，交 `src/main.cpp` 即可；文件末尾的 `--shot` 截图代码位于
> `#ifndef _WIN32` 中，Windows 编译时会被自动排除。
>
> 代码里用了 `fillroundrect`、`ExMessage` 等接口，建议 EasyX 2021 或更新版本。
> MinGW 版请使用 **Unicode 库** `lib64/libeasyxw.a`（CMake 已按 Unicode 配置好）。
>
> 注意：EasyX 的 MinGW 库与新版 UCRT 工具链可能存在 `__imp___iob_func` 链接问题；
> 如果遇到，直接改用上面的 Visual Studio 方案即可（老师环境一般就是 VS + EasyX）。

---

## 六、项目结构

```
homework/
├── CMakeLists.txt              # 构建脚本（macOS 兼容层 / Windows 官方 EasyX 自动切换）
├── src/
│   └── main.cpp                # 作业源码：画板界面 + 绘图逻辑（只使用 EasyX 接口）
├── compat/
│   ├── include/
│   │   ├── graphics.h          # EasyX 兼容层头文件（macOS）
│   │   ├── windows.h           # 让 #include <windows.h> 的示例代码可编译
│   │   └── conio.h             # 让 #include <conio.h> 的示例代码可编译
│   └── src/
│       └── easyx_mac.mm        # 兼容层实现：Cocoa 窗口 + CoreGraphics 绘图 + 消息转换
├── docs/
│   └── preview.png             # 界面预览图
└── README.md
```

`src/main.cpp` 里的主要结构：

* `ui` / `col`：界面尺寸与配色常量；
* `kPalette` / `kWidths` / `kToolNames`：调色板、线宽、工具名称；
* `Rect` / `Stroke`：按钮区域、一条笔画的数据结构；
* `PaintBoard`：布局、鼠标 / 键盘消息、绘制、撤销、清空等全部逻辑；
* `render()`：每次状态变化后重绘画布和工具栏（所以笔画可以撤销、可以实时预览）。

---

## 七、兼容层说明

### 已实现（本项目用到的部分）

* 窗口：`initgraph` / `closegraph` / `getwidth` / `getheight`
* 颜色：`setbkcolor` / `setlinecolor` / `setfillcolor` / `settextcolor`
* 颜色常量：`BLACK` / `WHITE` / `RED` / `GREEN` / `BLUE` / `YELLOW` 等
* 图形：`line` / `rectangle` / `fillrectangle` / `roundrect` / `fillroundrect`
  / `ellipse` / `fillellipse` / `circle` / `fillcircle` / `polyline` / `polygon` / `fillpolygon`
* 文字：`settextstyle` / `outtextxy` / `textwidth` / `textheight` / `setbkmode`
* 线型：`setlinestyle(PS_SOLID, 粗细)`
* 消息：`peekmessage` / `ExMessage`（鼠标、键盘，含 `mkLButton`、`vkcode` 等字段）
* 双缓冲：`BeginBatchDraw` / `FlushBatchDraw` / `EndBatchDraw`
* 其他：`Sleep`

> 用同学那份示例代码实测过：加上 `<conio.h>` / `<windows.h>` 的包含也不需要改，
> `msg.mkLButton`、`RED` 这些写法都能直接编译运行。

### 未实现

`IMAGE` 图片对象、图片读写、裁剪区域、旋转文字、鼠标滚轮、位运算绘图模式等。
这些接口在作业里没有用到；如果以后需要，可以照着 `easyx_mac.mm` 的写法继续补。

### 原理（三句话）

1. 所有绘图函数都画进一块内存位图（BGRA 像素），由 CoreGraphics 负责抗锯齿和中文文字；
2. 画完一帧把位图转成 `NSImage` 显示到 `NSImageView` 上；
3. Cocoa 的鼠标 / 键盘事件被转换成 EasyX 的 `WM_LBUTTONDOWN`、`WM_MOUSEMOVE` 等消息，
   由 `peekmessage` 交给作业代码，和 Windows 下的 `ExMessage` 用法一致。

### 调试截图模式（macOS）

```bash
./build/paint --shot docs/preview.png
```

不开窗口，直接把示例画面渲染成 PNG，方便检查界面布局。

---

## 八、常见问题

**Q：CLion 报 “CMake Error: No CMAKE_OBJCXX_COMPILER could be found.”**
安装 Xcode Command Line Tools：`xcode-select --install`，然后在 CLion 里
**Tools → CMake → Reset Cache and Reload Project**。

**Q：运行后只有 Dock 图标，没有窗口？**
程序必须在当前 macOS 桌面会话里运行（CLion 直接运行即可）。
通过 SSH 远程运行是看不到窗口的。

**Q：中文显示成方块？**
macOS 上使用系统字体 `PingFang SC`，不会缺字；
Windows 上使用 `Microsoft YaHei`，并已按 Unicode 编译，一般不会乱码。
`src/main.cpp` 已经保存为 **UTF-8 with BOM**；如果仍然乱码，再在项目属性里加 `/utf-8`。

**Q：在 Windows 上编译报 `fillroundrect` 找不到？**
EasyX 版本太旧，换 2021 或更新版本即可。

**Q：老师要求必须用 Visual Studio 工程提交怎么办？**
新建 VS 空项目，把 `src/main.cpp` 加进去即可，代码不需要改动。
`compat/` 目录只用于 Mac 本地调试，可以不提交。

---

## 九、说明

这套兼容层是为了让同学能在 Mac 上完成 EasyX 作业而写的，接口尽量贴近官方 EasyX；
它不能 100% 替代 EasyX（例如没有 `IMAGE`、`saveimage`），但覆盖了画板作业所需的全部功能。

Windows 上按第五节用 Visual Studio + 官方 EasyX 编译同一份源码即可。
