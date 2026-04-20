# Kate AI Ghost Overlay Renderer Design

## Background
用户选择将 ghost text 的渲染主路径收敛到 `editorWidget()` 上的 overlay，并要求 overlay 覆盖首行与后续行，同时在任意光标位置可见。

## Problem
`InlineNoteProvider` 参与行内布局，适合单行 inlay 类型用例。Copilot 式多行 ghost text 需要：
- 文档缓冲区保持不变
- 预览渲染不改变 Kate 的行布局
- 流式更新与滚动/重绘保持同步

## Questions and Answers
### Q1. overlay 在任意光标位置显示时，首行如何定位？
A1. 首行从 `view->cursorToCoordinate(anchor)` 映射到 `editorWidget()` 坐标后开始绘制。

### Q2. 后续行如何对齐？
A2. 后续行从 `view->textAreaRect().left()` 对齐，并保留建议文本自身缩进（空格/制表符字符留在文本里）。

### Q3. 行高来源是什么？
A3. 行高用 `cursorToCoordinate(line+1,0) - cursorToCoordinate(line,0)` 的 y 差值推导，回退到 `QFontMetrics(editorWidget->font()).height()`。

### Q4. 在行内位置插入预览时，如何处理后方真实文本？
A4. 当前版本采用纯叠加渲染，保持 Kate 文本可见性，ghost 颜色用 `Text/Base` 混合并降低 alpha。

## Design
新增 `GhostTextOverlayWidget`：
- 路径：`src/render/GhostTextOverlayWidget.{h,cpp}`
- 挂载：父对象为 `view->editorWidget()`，几何随父 widget 变化。
- 透明：`WA_TransparentForMouseEvents`、`WA_NoSystemBackground`、`WA_TranslucentBackground`
- 接口：
  - `void setState(const GhostTextState &state)`
  - `bool isActive() const`

绘制策略：
- 若 state 可见：拆分 `visibleText` 为逻辑行。
- 计算 anchor 位置（editorWidget 坐标）。
- clip 到 `textAreaRect` 映射后的矩形。
- 逐行绘制：
  - 第 0 行：x = anchorX
  - 第 1..N 行：x = textAreaLeft
  - y 以 anchorLine 的 top 为起点，每行增加 `lineHeightPx`。

EditorSession 集成：
- 用 overlay 替换 InlineNoteProvider 作为 ghost 渲染路径。
- onDeltaReceived/onRequestFinished/clearSuggestion/setSuppressed 时同步 overlay state。
- 监听 view 的滚动、display range 与 editorWidget resize，触发 overlay 更新。

## Implementation Plan
1. 新增 GUI 渲染测试 `GhostTextOverlayWidgetRenderingTest`，先写失败测试。
2. 实现 `GhostTextOverlayWidget`。
3. 修改 `EditorSession`：创建并维护 overlay，移除 InlineNoteProvider 渲染主路径。
4. 运行单测与全量 `ctest`。

## Examples
- anchor 在行中间：首行 ghost 从 cursor x 开始，后续行贴 textArea 左边。
- 文末位置：overlay 仍可绘制多行文本；可见范围受 viewport 高度裁剪。

## Trade-offs
- overlay 渲染不改变布局，滚动与输入保持稳定。
- 行内插入预览在视觉上表现为叠加，后方真实文本保持可见，信息密度更高。
