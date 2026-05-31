# SmartPianoQt v0.2.0-preview

版本定位：练习数据闭环版本。

这一版和 v0.1 的最大差异不是单个 UI 改动，而是已经形成完整链路：

```text
练习 -> 判定 -> 记录 -> 报告 -> 局部循环 -> 自动调速
```

## 新增

- 本地曲谱库模型 `LocalSheetModel`。
- SQLite 练习记录与练习报告。
- A-B 循环练习。
- 循环练习自动升速 / 降速。
- 节奏练习 `Perfect / Good / Early / Late / Missed` 判定。
- MIDI 输入 velocity 支持。
- 练习报告易错音、漏弹音、分数趋势。
- `PracticeSessionController` 拆分。
- QML 控制面板组件化。

## 修复 / 改进

- 更准确的有效练习时长。
- 更准确的 tempo map `offset_ms` 计算。
- 曲谱库与练习记录联动。
- CI 构建稳定性。

## 验证

- GitHub Actions CI：`build-and-test` 通过。
- 本地构建：`cmake --build build -j 4` 通过。
- 核心测试：`ctest --test-dir build --output-on-failure` 通过。
