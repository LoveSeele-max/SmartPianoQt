# SmartPianoQt

SmartPianoQt 是 Smart Piano 的 Qt 6 桌面版初版。当前目标不是一次性重写完整网页版，而是先把练琴链路跑通：曲谱加载、卷帘窗显示、虚拟键盘、自动播放、等待弹对的练习模式。

## 当前能力

- Qt Quick/QML 界面，包含控制面板、卷帘窗和 61 键虚拟键盘。
- C++ 播放状态机，支持播放、暂停、停止、拖动进度和 BPM 调整。
- 练习模式会停在当前音符或和弦，弹对后继续。
- 支持导入 JSON 和标准 MIDI 文件。
- 内置《小星星》示例曲。

## 本机环境

当前工程已在这套环境下验证通过：

- Qt 6.11.1 MinGW 64-bit：`D:\Qt\6.11.1\mingw_64`
- MinGW：`D:\mingw64\bin`
- CMake 4.3.1

如果 Qt 安装目录不同，可以在构建或运行前设置 `QT_PREFIX`。

## 构建

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=D:\Qt\6.11.1\mingw_64
cmake --build build --parallel 4
```

或者使用自己的 Qt 路径：

```powershell
$env:QT_PREFIX = "D:\Qt\6.11.1\mingw_64"
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=$env:QT_PREFIX
cmake --build build --parallel 4
```

## 运行

```powershell
.\run.ps1
```

如果 Qt 不在默认位置：

```powershell
$env:QT_PREFIX = "D:\Qt\6.11.1\mingw_64"
.\run.ps1
```

## JSON 曲谱格式

可以导入类似 [examples/twinkle.json](examples/twinkle.json) 的文件：

```json
{
  "name": "Twinkle",
  "bpm": 100,
  "data": [
    { "note": "C4", "duration": 1, "fingering": 1 },
    { "note": "G4", "duration": 1, "fingering": 5 }
  ]
}
```

也支持带绝对拍点的字段：

- `midi` 或 `note`
- `startTimeBeat`
- `durationBeat` 或 `duration`
- `velocity`
- `fingering`

## 下一步

- 接入 RtMidi，迁移 Yamaha PSR-E383 的 `0xFE/0xF8` 过滤。
- 接入 FluidSynth + SF2 音源。
- 增加 SQLite 曲谱库和练习记录。
- 增加 MusicXML / MXL 解析。
- 为 MIDI 解析、练习判断补单元测试。
