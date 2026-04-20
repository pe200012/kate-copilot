# InlineNote Rendering Experiment Design

## Background
当前插件的多行 ghost text 采用“首行 InlineNote + 后续行 overlay”。用户要求直接验证 KTextEditor::InlineNoteProvider 的真实渲染边界，重点观察三件事：
1. `inlineNoteSize()` 返回超过 `note.lineHeight()` 的高度时，Kate 实际会保留多少垂直空间。
2. `paintInlineNote()` 在单个 note 内自行绘制多行内容时，绘制区域是否跨出锚点行。
3. 将多个 notes 分别挂到后续真实行时，Kate 的表现是否接近“多行 ghost text”。

## Problem
现有结论主要来自 API 文档与生态用法。下一步需要一套可重复运行的实验，直接渲染 InlineNote，并用自动化测试验证截图里的实际像素分布。

## Questions and Answers
### Q1. 这次实验放在生产插件里还是测试里？
A1. 放在 `autotests/`。这样改动范围小，实验可重复，且不会影响现有用户路径。

### Q2. 这次实验验证什么层级？
A2. 验证真实的 KTextEditor 渲染结果。测试会创建 `KTextEditor::Document` 与 `KTextEditor::View`，注册自定义 `InlineNoteProvider`，抓取 editor widget 图像，再做像素断言。

### Q3. 哪些实验模式最关键？
A3. 三个模式足够回答当前问题：
- `OversizeSingleNote`：单个 note 请求 3 倍行高，并绘制三条彩色带。
- `MultilineSingleNote`：单个 note 内绘制多行文本与色带，观察裁剪。
- `PerLineNotes`：在锚点行与后续真实行分别返回 notes，观察真实效果。

## Design
新增一个 GUI 集成测试文件：`autotests/InlineNoteRenderingExperimentTest.cpp`。

测试内定义 `ProbeInlineNoteProvider`：
- `Mode mode`
- `QList<int> inlineNotes(int line) const`
- `QSize inlineNoteSize(const KTextEditor::InlineNote &note) const`
- `void paintInlineNote(...) const`
- 记录最近一次 `note.lineHeight()` 与 paint 调用信息，供断言使用

测试流程：
1. 创建 `QApplication` 下的临时顶层窗口。
2. `KTextEditor::Editor::instance()->createDocument()`，填充固定文本。
3. `document->createView(window)`，设置稳定尺寸与字体，`show()` 后等待布局稳定。
4. 注册 `ProbeInlineNoteProvider`。
5. 针对每种模式抓取 `editorWidget()->grab().toImage()`。
6. 通过强对比色块做像素断言：
   - oversize note 的可见色块高度应落在单行高度内。
   - multiline single note 的第二、三条色带应被裁剪掉。
   - per-line notes 的色块应分别出现在多个真实行上。
7. 将截图保存到临时目录，便于人工复核。

## Implementation Plan
1. 在 `autotests/CMakeLists.txt` 新增 GUI 集成测试目标，链接 `KF6::TextEditor`、`Qt6::Widgets`、`Qt6::Gui`、`Qt6::Test`。
2. 先写失败测试：创建测试骨架，声明三条断言，运行单测，确认失败。
3. 在测试文件内补齐 `ProbeInlineNoteProvider` 与渲染辅助逻辑。
4. 运行目标测试，确认通过。
5. 运行全量 `ctest --test-dir build --output-on-failure`，确认整个工程保持绿色。
6. 在本设计文档末尾追加实验结果与截图路径。

## Examples
### ✅ OversizeSingleNote
- `inlineNoteSize()` 返回 `QSize(160, note.lineHeight() * 3)`
- `paintInlineNote()` 依次绘制洋红 / 橙 / 青三条横带
- 预期：截图里只有首条横带完整可见

### ✅ PerLineNotes
- 第 1 个 note 在锚点行，颜色洋红
- 第 2 个 note 在下一真实行，颜色绿色
- 第 3 个 note 在下下真实行，颜色蓝色
- 预期：三种颜色分别出现在三条真实行中

### ❌ 依赖文档推断结论
- 只读 API 注释
- 不做实际渲染
- 不抓图

## Trade-offs
- GUI 像素测试比纯逻辑测试更脆弱，收益是结论直接绑定 Kate 的真实绘制结果。
- 实验代码放在测试里，生产插件保持干净；后续若需要可将探针提炼为独立工具。

## Implementation Results
- 已新增 `autotests/InlineNoteRenderingExperimentTest.cpp`。
- 已在 `autotests/CMakeLists.txt` 注册 `kateaiinlinecompletion_inline_note_rendering_experiment_test`。
- 实测截图输出到 `/tmp/kate-ai-inline-note-rendering/`：
  - `oversize_single_note.png`
  - `multiline_single_note.png`
  - `per_line_notes.png`
- 自动化断言结果：
  - `OversizeSingleNote`：provider 请求 `3 * lineHeight` 的高度时，截图里只有首条洋红色带可见。
  - `MultilineSingleNote`：同一个 note 内绘制的第二、第三条色带完全被裁剪。
  - `PerLineNotes`：洋红 / 绿 / 蓝三条色带分别出现在三个真实行中。
- 结论：
  - `InlineNoteProvider` 可以在单行内预留水平空间。
  - 单个 note 的可见绘制区域受单行高度约束。
  - 多行效果只能通过“给多个真实行分别返回 note”来获得；它表现为多条真实行各自带内联附注，无法形成“在锚点之后插入虚拟行并整体下推后文”的布局。
- 验证结果：
  - `ctest --test-dir build -R kateaiinlinecompletion_inline_note_rendering_experiment_test --output-on-failure`：1/1 passed
  - `ctest --test-dir build --output-on-failure`：5/5 passed
