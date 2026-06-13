# SmartPianoQt

SmartPianoQt 是 Smart Piano 的 Qt 6 桌面版。当前版本已经跑通“练习数据闭环”：曲谱加载、卷帘窗显示、虚拟键盘、自动播放、等待练习、节奏练习、练习记录、练习报告、A-B 局部循环和自动调速。

## 当前能力

- Qt Quick/QML 界面，包含控制面板、卷帘窗和 61 键虚拟键盘。
- QML 控制面板已拆分为播放控制、本地曲谱库、MIDI 输入、练习状态和练习报告等组件。
- C++ 播放状态机，支持播放、暂停、停止、拖动进度、播放速度和音量调节。
- 通过 Qt Multimedia 内置柔和钢琴合成器播放，比系统 General MIDI 更稳定。
- 如果 `soundfonts/` 中存在 `.sf2/.sf3` 且本机可加载 FluidSynth DLL，会优先使用真实采样钢琴音色。
- 等待练习模式会停在当前音符或和弦，弹对后继续。
- 节奏练习支持 `Perfect / Good / Early / Late / Missed` timing score 判定。
- 支持 A-B 循环练习：设置循环起点/终点后只练局部区间，到 B 点自动回到 A 点。
- 循环练习支持自动调速：连续 3 次全对自动升速，错 2 次自动降速。
- 支持导入 JSON 和标准 MIDI 文件。
- MIDI 解析支持 tempo map 保留、延音踏板、缺失 NoteOff 兜底，并有基础 parser 测试覆盖。
- 自动播放由独立 PlaybackEngine 按 MIDI tempo map 推进，变速 MIDI 可按曲谱速度变化播放，并支持 50% 到 150% 整体变速练习。
- 支持本地 MIDI 曲谱库模型 LocalSheetModel：把 `.mid/.midi` 文件放进 `midi_library/` 后刷新即可加载，并能识别已练过的曲谱。
- SQLite 练习记录已接入：记录曲谱、session、判定事件、有效练习时长和 offset_ms。
- PracticeReportPanel 会显示最近练习记录、最近 5 次分数趋势、易错音 Top、漏弹音 Top 和练习建议。
- 左侧 MIDI 输入面板已支持 Windows MIDI 设备列表、连接入口、velocity 传递和 `0xFE/0xF8` 实时消息过滤。
- PracticeSessionController 已拆分 session 生命周期，负责 begin / append event / finish。
- GitHub Actions CI 会自动构建并运行核心测试。
- 内置《小星星》示例曲。

## 开发环境

推荐和已验证的环境：

- 本地开发：Qt 6.11.1 + MinGW 64-bit
- CI 验证：Qt 6.6.3 + Visual Studio 2022 x64
- 最低 Qt 版本：Qt 6.5，对应 [CMakeLists.txt](CMakeLists.txt) 中的 `find_package(Qt6 6.5 ...)`
- 最低 CMake 版本：3.21

如果 Qt 不在系统默认搜索路径里，建议设置：

```powershell
$env:QT_PREFIX = "C:\Qt\6.11.1\mingw_64"
```

MSVC / CI 风格构建也可以使用 `QT_ROOT_DIR` 或 `CMAKE_PREFIX_PATH`。

## 构建

推荐使用 [CMakePresets.json](CMakePresets.json)：

```powershell
$env:QT_PREFIX = "C:\Qt\6.11.1\mingw_64"
cmake --preset windows-mingw-debug
cmake --build --preset windows-mingw-debug
```

Release / CI 风格构建：

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

如果不用 presets，也可以继续手动指定：

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=$env:QT_PREFIX
cmake --build build --parallel 4
```

## 测试

```powershell
ctest --test-dir build --output-on-failure
```

使用 presets 时：

```powershell
ctest --preset windows-mingw-debug
```

当前测试覆盖 NoteUtils、JSON parser、MIDI parser、MIDI 输入过滤、tempo map 播放推进、PlaybackEngine 状态推进、PracticeEngine 核心判定、节奏 timing score、SQLite 练习记录和练习报告查询。

测试源码按领域拆分在 [tests](tests) 目录下，`test_core.cpp` 只保留统一 runner。

## 运行

```powershell
.\run.ps1
```

`run.ps1` 会按以下顺序查找运行环境：

- `SMARTPIANO_EXE` 或常见 build 输出目录里的 `SmartPianoQt.exe`
- `QT_PREFIX`、`QT_ROOT_DIR`、`CMAKE_PREFIX_PATH` 或常见 Qt 安装目录
- `MINGW_BIN`、`MINGW_PREFIX` 或常见 MinGW / MSYS2 / Qt Tools 目录
- `FLUIDSYNTH_DLL`、`FLUIDSYNTH_PREFIX` 或常见 FluidSynth 安装目录

常用示例：

```powershell
$env:QT_PREFIX = "C:\Qt\6.11.1\mingw_64"
.\run.ps1 -BuildDir build\mingw-debug
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

## 本地 MIDI 库

把下载好的 `.mid` 或 `.midi` 文件放到项目根目录的 [midi_library](midi_library) 文件夹里。应用启动时会自动扫描，也可以在左侧面板点击“刷新”重新读取。

为了避免误提交曲谱文件，`midi_library/*.mid` 和 `midi_library/*.midi` 已加入 `.gitignore`。

## MIDI 输入

当前 Windows 构建会通过系统 MIDI API 枚举输入设备，连接后会把 NoteOn / NoteOff 转给练习判定链路。实时消息 `0xFE` Active Sensing 和 `0xF8` Timing Clock 会被过滤。

后续如果接入 RtMidi，可以替换 `src/midi/MidiInputService.*` 的底层枚举和打开设备实现，QML 和 `PianoController` 接口不用再大改。

## 钢琴音色

最接近传统钢琴的方式是使用真实采样 SoundFont。内置柔和钢琴只作为无外部依赖的保底音色：

1. 安装 FluidSynth，或把 `libfluidsynth-3.dll` 放到 `SmartPianoQt.exe` 旁边。
2. 把钢琴 `.sf2` 或 `.sf3` 文件放到 [soundfonts](soundfonts) 文件夹。
3. 在应用的“设备”页查看当前音色，也可以手动选择 SoundFont、重新扫描 `soundfonts/`，并立即试听当前音色。

如果没有检测到 FluidSynth 或 SoundFont，应用会自动退回内置柔和钢琴合成器，并在音源状态中显示失败原因或当前后端。

设备页还提供力度曲线和音频延迟模式：

- `线性`：保持 MIDI velocity 原始力度。
- `柔和`：压低中高力度，适合长时间练习。
- `明亮`：抬高中等力度，触键反馈更清楚。
- `压缩`：收敛最轻和最重力度，适合输出力度不稳定的键盘。
- `低延迟 / 稳定 / 兼容`：调整音频 buffer，实时弹奏优先低延迟，自动播放或低配置机器可选稳定/兼容。

`run.ps1` 会在启动时自动连接 SoundFont：如果 [soundfonts](soundfonts) 里还没有 `.sf2/.sf3`，会优先从 `SMARTPIANO_SOUNDFONT`、`SOUNDFONT_PATH`、`Documents\SoundFonts` 和 `Downloads` 寻找音色文件，并把它链接到项目的 `soundfonts/` 目录。

如果 SoundFont 在别的位置，可以设置：

```powershell
$env:SMARTPIANO_SOUNDFONT = "C:\path\to\piano.sf2"
```

如果 FluidSynth 安装在自定义位置，可以设置：

```powershell
$env:FLUIDSYNTH_PREFIX = "C:\path\to\fluidsynth"
```

参考入口：

- FluidSynth 下载：https://www.fluidsynth.org/download/
- MuseScore SoundFont 说明：https://musescore.org/en/handbook/4/soundfonts

## 已知限制

- MIDI 输入后端当前主要面向 Windows WinMM，macOS / Linux 输入设备支持仍在路线图中。
- 目前还没有正式安装包或 Release artifact，需要从源码构建运行。
- FluidSynth 和 SoundFont 是可选增强；缺失时会自动使用内置合成器。
- 示例曲和测试覆盖还在扩展中，复杂 MIDI 文件仍建议保留人工验收。
- README 截图 / GIF、FAQ 和完整架构图后续补充。

## License

本项目使用 [MIT License](LICENSE)。

## 下一步

- 用 RtMidi 替换当前 Windows MIDI 输入后端，提升跨平台支持。
- 打包 FluidSynth 运行库和推荐的开源钢琴 SoundFont。
- 增加 MusicXML / MXL 解析。
- 做小节级统计、错误集中区域分析和自动 A-B 片段建议。
- 增加更完整的练习报告页面，例如提前/滞后分布、最容易错的小节和长期趋势。
- 为 A-B 循环、自动调速和 MIDI 输入增加更完整的自动化/人工验收脚本。
