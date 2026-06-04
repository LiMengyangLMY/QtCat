# QtCat

一个基于 Qt Widgets 的桌面宠物 demo。

## 技术栈

- C++17
- Qt 5 Widgets
- qmake
- ffmpeg

## 功能

- 从 `静止1.mp4` 提取一帧生成 `256x256` 的静止态 PNG
- 从 `旋转1.mp4` 以 `15fps` 提取一组 `256x256` 的旋转序列 PNG
- 桌宠默认显示静止态，鼠标悬停时循环播放旋转动画
- 左键拖拽移动位置，右键弹出菜单退出

## 构建

```bash
./scripts/build.sh
```

生成的可执行文件位于 `dist/QtCatPet`。
