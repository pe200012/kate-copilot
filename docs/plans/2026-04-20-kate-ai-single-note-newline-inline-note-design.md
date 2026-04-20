# Kate AI Single-Note Newline InlineNote Design

## Background
当前多行 ghost text 主路径采用“多条 InlineNote 映射到连续真实行”。最新实验证明另一条路线也成立：单个 InlineNote 在 `paintInlineNote()` 中调用 `QPainter::drawText(rect, flags, text)` 且 `text` 含有 `\n` 时，Qt 会进行多行布局，绘制结果会向后续行区域延展。

## Problem
我们要把运行时主路径切到“单个锚点 note + 完整 suggestion 文本”这条路线，同时确认两个关键点：
1. 流式更新从单行扩展到多行时，Kate 是否会稳定重绘后续溢出区域。
2. 哪种刷新策略更适合这条路线：`inlineNotesChanged(anchorLine)`，还是 `inlineNotesReset()`。

## Questions and Answers
### Q1. note 的数量采用什么模型？
A1. 采用单 note 模型。`inlineNotes(int line)` 只在锚点行返回一个 note。

### Q2. note 中绘制什么文本？
A2. 直接绘制完整的 `visibleText`，保留其中的 `\n` 与缩进空白。

### Q3. `inlineNoteSize()` 返回什么尺寸？
A3. 高度先保持 `note.lineHeight()`，宽度取各逻辑行像素宽度的最大值。这样可以观察“单行高度 + 多行文本溢出”的真实效果。

### Q4. 刷新信号采用什么策略？
A4. 先用测试验证。若 `inlineNotesChanged(anchorLine)` 足以驱动后续行重绘，则采用它；若后续溢出区域需要更强的重绘触发，则切到 `inlineNotesReset()`。

## Design
`GhostTextInlineNoteProvider` 切换为单 note 渲染：
- `inlineNotes(line)`：仅当 `line == anchor.line` 时返回 `{anchor.column}`。
- `inlineNoteSize(note)`：
  - 按 `visibleText.split('\n', Qt::KeepEmptyParts)` 计算最大行宽。
  - 高度返回 `note.lineHeight()`。
- `paintInlineNote(note, painter, direction)`：
  - 直接把完整 `visibleText` 传给 `drawText(rect, flags, text)`。
  - 先验证 `Qt::AlignLeft | Qt::AlignTop` 与当前效果的匹配度；目标是首行从锚点行顶部开始，后续文本自然向下溢出。
- `setState()`：依据动态渲染测试结果选择 `inlineNotesChanged(anchorLine)` 或 `inlineNotesReset()`。

## Implementation Plan
1. 新增动态刷新 GUI 测试，验证单 note newline 字符串从单行更新到多行时的重绘结果。
2. 比较 `inlineNotesChanged(anchorLine)` 与 `inlineNotesReset()` 的效果。
3. 修改 `GhostTextInlineNoteProvider` 为单 note newline 策略。
4. 更新单元测试与渲染测试。
5. 运行全量构建与测试。
6. 在本文档末尾追加实现结果与偏差说明。

## Examples
### ✅ 例 1
- anchor: `(10, 8)`
- visibleText: `"foo()\n    bar();\n}"`
- note: line 10, column 8, text 为完整三行字符串

### ✅ 例 2
- anchor: `(3, 4)`
- visibleText: `"\n    return x;"`
- note: line 3, column 4, text 为两行字符串

### ❌ 例 3
- 继续把多行 suggestion 拆成多个 notes
- 这条路线仍可用，当前实验目标是验证更简洁的单 note 模型

## Trade-offs
- 单 note 模型更简单，代码量更小，接近你刚才提出的思路。
- 文本块向后续行溢出的行为建立在 Qt 文本布局与 Kate 行级裁切的叠加效果上，稳定性需要靠真实渲染测试验证。
- 这条路线对文末场景更有吸引力，因为它减少了对“后续真实行数量”的依赖。
## Implementation Results
- 已将 `GhostTextInlineNoteProvider` 切到单 note newline 策略。
- 运行时行为：
  - `inlineNotes(line)` 仅在锚点行返回 `{anchor.column}`。
  - `inlineNoteSize()` 宽度取 suggestion 各逻辑行的最大像素宽度，高度保持 `note.lineHeight()`。
  - `paintInlineNote()` 直接用 `painter.drawText(rect, hAlign | Qt::AlignVCenter, m_state.visibleText)` 绘制完整多行字符串。
- 刷新策略选择：
  - 锚点未变化且可见状态未变化时，发 `inlineNotesChanged(anchorLine)`。
  - 锚点变化或可见状态切换时，发 `inlineNotesReset()`。
- 动态渲染结论：
  - `Qt::AlignVCenter` 能让单 note 中的多行文本向后续行区域延展。
  - `inlineNotesChanged(anchorLine)` 足以驱动运行中的单行→多行扩展刷新。
- 更新测试：
  - `GhostTextInlineNoteProviderTest` 改为验证“单锚点 note + 锚点行刷新”。
  - `GhostTextInlineNoteProviderRenderingTest` 覆盖：
    - 真实 provider 从单行扩展到多行
    - 单 note newline 字符串的静态渲染
    - 单 note newline 字符串的动态更新，分别验证 `inlineNotesChanged(anchorLine)` 与 `inlineNotesReset()`
- 关键调试截图：
  - `/tmp/kate-ai-inline-note-rendering/single_note_newline_string.png`
  - `/tmp/kate-ai-inline-note-rendering/single_note_newline_dynamic_changed_line_multiline.png`
  - `/tmp/kate-ai-inline-note-rendering/single_note_newline_dynamic_reset_multiline.png`
- 偏差说明：
  - 这条实验主路径依赖 Qt 多行文本块的溢出效果，布局语义比“每行一个 note”更隐式。
- 验证结果：
  - `cmake --build build -j 8 && ctest --test-dir build --output-on-failure`：7/7 passed
