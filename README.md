# SmartPianoQt

SmartPianoQt 是 Smart Piano 的 Qt 6 桌面版初版。当前目标不是一次性重写完整网页版，而是先把练琴链路跑通：曲谱加载、卷帘窗显示、虚拟键盘、自动播放、等待弹对的练习模式。

## 当前能力

- Qt Quick/QML 界面，包含控制面板、卷帘窗和 61 键虚拟键盘。
- C++ 播放状态机，支持播放、暂停、停止、拖动进度、BPM 调整和音量调节。
- 通过 Qt Multimedia 内置柔和钢琴合成器播放，比系统 General MIDI 更稳定。
- 如果 `soundfonts/` 中存在 `.sf2/.sf3` 且本机可加载 FluidSynth DLL，会优先使用真实采样钢琴音色。
- 练习模式会停在当前音符或和弦，弹对后继续。
- 支持导入 JSON 和标准 MIDI 文件。
- MIDI 解析支持 tempo map 保留、延音踏板、缺失 NoteOff 兜底，并有基础 parser 测试覆盖。
- 自动播放会按 MIDI tempo map 推进，变速 MIDI 可按曲谱速度变化播放。
- 支持本地 MIDI 曲谱库：把 `.mid/.midi` 文件放进 `midi_library/` 后刷新即可加载。
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

## 测试

```powershell
ctest --test-dir build --output-on-failure
```

当前测试覆盖 NoteUtils、JSON parser、MIDI tempo map、sustain pedal、缺失 NoteOff 兜底、异常 VLQ 和 tempo map 播放推进。

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

## 本地 MIDI 库

把下载好的 `.mid` 或 `.midi` 文件放到项目根目录的 [midi_library](midi_library) 文件夹里。应用启动时会自动扫描，也可以在左侧面板点击“刷新”重新读取。

为了避免误提交曲谱文件，`midi_library/*.mid` 和 `midi_library/*.midi` 已加入 `.gitignore`。

## 钢琴音色

最接近传统钢琴的方式是使用真实采样 SoundFont：

1. 安装 FluidSynth，或把 `libfluidsynth-3.dll` 放到 `SmartPianoQt.exe` 旁边。
2. 把钢琴 `.sf2` 或 `.sf3` 文件放到 [soundfonts](soundfonts) 文件夹。
3. 重新启动应用，顶部状态会显示 `钢琴音色：SoundFont 采样 - 文件名`。

如果没有检测到 FluidSynth 或 SoundFont，应用会自动退回内置柔和钢琴合成器。

`run.ps1` 会在启动时自动连接 SoundFont：如果 [soundfonts](soundfonts) 里还没有 `.sf2/.sf3`，会优先从 `SMARTPIANO_SOUNDFONT`、`SOUNDFONT_PATH` 或下面这个本机目录寻找音色文件，并把它链接到项目的 `soundfonts/` 目录：

```text
E:\UprightPianoKW-SF2-20220221
```

如果 SoundFont 在别的位置，可以设置：

```powershell
$env:SMARTPIANO_SOUNDFONT = "D:\path\to\piano.sf2"
```

FluidSynth 本机运行库也已支持默认识别这个路径：

```text
E:\fluidsynth-v2.5.4-win10-x64-cpp11
```

如果 FluidSynth 安装在别的位置，可以设置：

```powershell
$env:FLUIDSYNTH_PREFIX = "E:\fluidsynth-v2.5.4-win10-x64-cpp11"
```

参考入口：

- FluidSynth 下载：https://www.fluidsynth.org/download/
- MuseScore SoundFont 说明：https://musescore.org/en/handbook/4/soundfonts

## 下一步

- 接入 RtMidi，迁移 Yamaha PSR-E383 的 `0xFE/0xF8` 过滤。
- 打包 FluidSynth 运行库和推荐的开源钢琴 SoundFont。
- 增加 SQLite 曲谱库和练习记录。
- 增加 MusicXML / MXL 解析。
- 为 PracticeEngine / seek / 练习判定补单元测试。
- 拆出 PlaybackEngine / PracticeEngine，让 `PianoController` 更专注于 QML facade。
