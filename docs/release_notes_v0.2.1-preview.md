# SmartPianoQt v0.2.1-preview

版本定位：练习数据闭环的小版本预览。

## 新增

- 本地曲谱库 `LocalSheetModel`。
- A-B 循环练习。
- 循环练习自动升速 / 降速。
- 更细的节奏练习判定：`Perfect / Good / Early / Late / Missed`。
- `PracticeSessionController`，有效练习时长统计。
- 练习报告数据增强。

## 修复 / 改进

- 本地曲谱库和 SQLite 练习记录联动。
- 暂停时不再污染有效练习时长。
- QML 和 Controller 接线优化。
