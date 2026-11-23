# QtCat

一个基于 Qt Widgets 的桌面宠物 demo。

## 技术栈

- C++17 / Qt 5.15 Widgets / qmake
- ffmpeg + Python (Pillow)：从视频提取并抠图生成 PNG 素材

## 功能

- 从 `media/静止1.mp4` 提取一帧，生成 `256x256` 的静止态 PNG
- 从 `media/旋转1.mp4` 以 `15fps` 提取旋转序列 PNG（`256x256`，自动去除黑色背景）
- 桌宠默认显示静止态，鼠标悬停时循环播放旋转动画
- 无边框、透明背景、窗口置顶，左键拖拽移动位置
- 右键弹出菜单（点击“退出”即可退出程序）

### 功能：摸鱼看小说

- 支持导入主流电子书格式：**`.txt`、`.epub`、`.mobi`、`.azw`、`.azw3`**
  - **EPUB**：自动解析元数据与章节，流式提取干净正文；
  - **MOBI / AZW / AZW3**：内置 PalmDOC LZ77 解码，提取无 DRM 保护的完整电子书内容；
  - **TXT**：智能嗅探 UTF-8 与 GB18030/GBK 编码，杜绝乱码。
- **两种导入方式**：直接把电子书拖到猫身上（零弹窗极简），或右键打开“摸鱼书架”面板浏览与挑选历史记录。
- **视觉与交互**：猫本体下方显示等宽（256px）、高度为猫 50%（128px）的全透明文本框；支持纯白到纯黑多档颜色、字号调节、鼠标滚轮翻行，以及双击/Esc 老板键一键隐蔽。

## 目录结构

```
QtCat/
├─ DesktopPet.pro          # qmake 工程文件
├─ README.md
├─ docs/                   # 需求、方案与操作文档
│  ├─ prompt.md            #   原始需求
│  ├─ 操作文档.md           #   编译 / 运行步骤
│  └─ 摸鱼看小说方案.md     #   “摸鱼看小说”功能与导入界面设计方案
├─ media/                  # 原始视频素材（素材提取的输入）
├─ resources/
│  ├─ resources.qrc        # Qt 资源清单（编译进 exe）
│  └─ assets/              # 提取生成的 PNG：idle/、rotate/
├─ src/                    # C++ 源码
│  ├─ main.cpp
│  └─ desktoppet.h/.cpp    #   桌宠窗口：显示、动画、拖拽
├─ scripts/                # 环境安装、素材提取、构建脚本
├─ build/                  # 编译中间产物（自动生成，已忽略）
├─ dist/                   # 可直接运行的发布目录（自动生成，已忽略）
└─ third_party/qt/         # 项目内 Qt + MinGW 工具链（自动安装，已忽略）
```

## 脚本说明

| 脚本 | 作用 |
| --- | --- |
| `scripts/install_tools.ps1` | Windows：安装 Qt 5.15.2 (MinGW) 到 `third_party/qt`，并安装 aqtinstall、Pillow |
| `scripts/extract_assets.ps1` / `.sh` | 用 ffmpeg 从 `media/` 视频提取帧，调用 `remove_black_background.py` 抠图，输出到 `resources/assets/` |
| `scripts/remove_black_background.py` | 去除与边缘相连的黑色背景并缩放到指定尺寸 |
| `scripts/build.ps1` / `.sh` | 提取素材 → qmake → make → 拷贝到 `dist/`（Windows 还会执行 windeployqt） |

## 构建与运行

### Windows

```powershell
# 首次：安装工具链（约 1GB）
powershell -ExecutionPolicy Bypass -File .\scripts\install_tools.ps1

# 构建（需要 ffmpeg 已加入 PATH）
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1

# 素材已存在时可跳过提取，无需 ffmpeg / Python
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -SkipAssets

# 运行
.\dist\QtCatPet.exe
```

### Linux

```bash
# 依赖：qt5 开发包 (qmake)、ffmpeg、python3-pil
./scripts/build.sh
SKIP_ASSETS=1 ./scripts/build.sh   # 跳过素材提取
./dist/QtCatPet
```

## 修改素材

1. 替换 `media/` 下的视频（文件名保持不变）；
2. 如果帧数有变化，同步更新 `resources/resources.qrc` 中的 `frame_XX.png` 列表；
3. 重新执行构建脚本（不要加跳过素材提取的选项）。
