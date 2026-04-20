2026-04-20：单个 InlineNote + 内嵌换行字符串实验。

- 基于 Qt 文档：`QPainter::drawText(const QRect&, int flags, const QString&)` 支持在矩形内绘制带 `\n` 的多行文本。
  - 参考：Qt QPainter 文档 https://doc.qt.io/qt-6.11/qpainter.html
- 新增/扩展测试：`autotests/GhostTextInlineNoteProviderRenderingTest.cpp`
  - 用自定义 `SingleNoteNewlineStringProvider` 只在锚点行返回一个 note。
  - `inlineNoteSize()` 返回单行高度。
  - `paintInlineNote()` 调用 `painter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, "first\n    second\nthird")`。
- 调试截图：`/tmp/kate-ai-inline-note-rendering/single_note_newline_string.png`
- 实测结果：
  - 单个 note 中的带换行字符串会向后续行溢出。
  - 画面里能看到 `first` 与 `second`；更深的后续行进入裁切区，表现为部分或全部不可见。
- 结论：
  - 这条路线在 Kate 中确实会产生“一个 note 画出多行 ghost”的效果。
  - 它绕开了“每个真实行一个 note”的显式模型，布局行为更像 Qt 多行文本块 + KTextEditor 行级裁切的叠加结果。
  - 作为技巧可用，稳定性需要继续观察滚动、重绘、不同字体和 HiDPI 下的表现。
- 验证：`cmake --build build -j 8 && ctest --test-dir build --output-on-failure` 7/7 passed。