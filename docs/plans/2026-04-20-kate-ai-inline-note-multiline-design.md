# Kate AI InlineNote Multiline Design

## Background
用户选择将多行 ghost text 主路径收敛到 `KTextEditor::InlineNoteProvider`。现有实现采用“首行 InlineNote + 后续行 overlay”。我们已经通过 `InlineNoteRenderingExperimentTest` 验证：InlineNote 可以在连续真实行上稳定渲染多条附注。

## Problem
当前 `GhostTextInlineNoteProvider` 只渲染首行：
- `inlineNotes(int line)` 只在锚点行返回 note
- `inlineNoteSize()` / `paintInlineNote()` 只使用首行文本
- `setState()` 只更新锚点行
- `EditorSession` 仍创建并同步 `GhostTextPushDownOverlay`

目标是把多行建议拆成“首行 + 后续真实行”的多条 inline notes，并让后续行按建议文本自身缩进显示。

## Questions and Answers
### Q1. 后续 ghost 行的起始列采用什么规则？
A1. 采用“按建议文本自身缩进”。这意味着后续行从列 0 挂 note，缩进空白保留在 note 文本里。

### Q2. 为什么后续行从列 0 挂 note？
A2. 这样 Kate 会把整条真实行正文整体右移，视觉上更接近“插入了一条新行”。若挂在缩进列，真实行前缀会保留在左侧，观感不符合插入行。

### Q3. 超出文档末尾的建议行如何处理？
A3. 当前版本仅在已有真实行上显示 inline notes。超过文档尾部的额外建议行先不显示，接受写回仍按完整建议插入。

## Design
`GhostTextInlineNoteProvider` 将 `visibleText` 按 `\n` 拆分为逻辑行：
- 第 0 行：note 挂在 `anchor.column`
- 第 1..N 行：note 挂在后续真实行的列 0
- 每个 note 只绘制自己对应的一行文本

Provider 需要新增按文档行索引取文本的辅助函数，并让 `inlineNoteSize()` / `paintInlineNote()` 基于 `note.position().line()` 选择对应建议行。

状态刷新规则改为按“受影响行并集”发 `inlineNotesChanged(line)`：
- 旧状态可见范围：`[prevAnchorLine, prevAnchorLine + prevLineCount - 1]`
- 新状态可见范围：`[nextAnchorLine, nextAnchorLine + nextLineCount - 1]`
- 对并集中的每一行发 `inlineNotesChanged`

`EditorSession` 从主路径移除 overlay：
- 不再创建 `GhostTextPushDownOverlay`
- 不再同步 overlay state
- 保留现有 `Tab` / `Esc` / generation / request lifecycle

## Implementation Plan
1. 新增 `GhostTextInlineNoteProviderTest`，先覆盖多行 note 列位置与受影响行刷新。
2. 修改 `GhostTextInlineNoteProvider`，让多行 suggestion 映射到连续真实行。
3. 运行单测，确认红转绿。
4. 修改 `EditorSession`，移除 overlay 主路径接线。
5. 运行全量构建与测试。
6. 在本设计文档末尾追加实现结果。

## Examples
### ✅ 例 1
- anchor: `(10, 8)`
- visibleText: `"foo()\n    bar();\n}"`
- notes:
  - line 10 -> column 8 -> `foo()`
  - line 11 -> column 0 -> `    bar();`
  - line 12 -> column 0 -> `}`

### ✅ 例 2
- anchor: `(3, 4)`
- visibleText: `"\n    return x;"`
- notes:
  - line 3 -> column 4 -> ``
  - line 4 -> column 0 -> `    return x;`

### ❌ 例 3
- 后续行挂在缩进列
- 左侧仍保留真实行前缀文本
- 视觉上像“在真实行中间插入一段说明”，不够接近 ghost insertion

## Trade-offs
- InlineNote 主路径保持 Kate 原生绘制与滚动同步，观感更统一。
- 后续行依附真实行显示，无法创造真正的虚拟空白行。
- 超出文档尾部的建议行需要后续单独设计。
## Implementation Results
- 已新增 `autotests/GhostTextInlineNoteProviderTest.cpp`。
- 已在 `autotests/CMakeLists.txt` 注册 `kateaiinlinecompletion_ghost_text_inline_note_provider_test`，并直接编译 `src/render/GhostTextInlineNoteProvider.cpp` 进入测试目标。
- `GhostTextInlineNoteProvider` 已改为：
  - 按 `Qt::KeepEmptyParts` 拆分 `visibleText`
  - 首行 note 挂在 `anchor.column`
  - 后续行 note 挂在后续真实行的列 `0`
  - `inlineNoteSize()` / `paintInlineNote()` 按 `note.position().line()` 选择对应建议行文本
  - `setState()` 对旧/新可见范围并集逐行发 `inlineNotesChanged(line)`
- `EditorSession` 已移除 overlay 主路径接线：
  - 不再创建 `GhostTextPushDownOverlay`
  - 不再同步 overlay state
  - InlineNote 成为唯一渲染路径
- 偏差说明：
  - overlay 文件与编译单元仍保留在仓库中，当前版本它们已退出运行时主路径。
- 追加调试验证：
  - 已新增 `autotests/GhostTextInlineNoteProviderRenderingTest.cpp`，直接创建 `KTextEditor::View` + `GhostTextInlineNoteProvider`，验证“运行中从单行扩展到多行”会把新增 ghost 行画到后续真实行上。
  - 调试截图：
    - `/tmp/kate-ai-inline-note-rendering/ghost_inline_single_line.png`
    - `/tmp/kate-ai-inline-note-rendering/ghost_inline_multiline.png`
  - 该测试表明：当锚点下方已经存在真实行时，InlineNote 多行动态更新可以工作。
  - 这把当前用户报告进一步收敛到一个已知边界：超出文档末尾的 ghost 行没有真实行承载时，视图只能显示首行 suggestion。
- 追加单 note + 内嵌换行实验：
  - 同一测试文件中新增 `singleNoteNewlineStringSpillsIntoFollowingLines()`。
  - 调试截图：`/tmp/kate-ai-inline-note-rendering/single_note_newline_string.png`
  - 实测行为：单个 InlineNote 若在 `paintInlineNote()` 中调用 `painter.drawText(rect, ..., "first\n    second\nthird")`，Qt 会把 `\n` 解释为多行布局，Kate 画面会出现向后续行溢出的文本。
  - 观察到的结果接近“首行 + 次行可见，后续更深行逐步被裁切”的形态，因此这条路线具备可利用性，也带有一定布局不确定性。
- 验证结果：
  - `ctest --test-dir build -R kateaiinlinecompletion_ghost_text_inline_note_provider_test --output-on-failure`：1/1 passed
  - `ctest --test-dir build -R kateaiinlinecompletion_ghost_text_inline_note_provider_rendering_test --output-on-failure`：1/1 passed
  - `cmake --build build -j 8 && ctest --test-dir build --output-on-failure`：7/7 passed
