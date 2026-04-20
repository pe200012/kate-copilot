# Kate AI Overlay Stabilization and Release Prep Design

## Background
当前插件已经把多行 ghost text 主路径切到 `editorWidget()` overlay，并且完成了基础 SSE、Ollama、Copilot OAuth、prompt 回归评测与基础 GUI 渲染验证。

当前阶段的目标有两组：
1. 把 overlay 路线推进到可日常使用的状态。
2. 补齐面向使用者的文档与设置页提示，让构建、安装、配置、试跑路径清晰可复现。

官方资料与实测给出三条直接约束：
- `KTextEditor::Document::newMovingCursor()` 适合维护随文档编辑自动移动的锚点。
- `KTextEditor::Document::EditingTransaction` 提供编辑事务边界，适合接受补全时维持清晰的编辑生命周期。
- `QPainter::drawText(QPoint, text)` 的 y 值使用 font baseline，适合做比 `AlignVCenter` 更可控的基线绘制。

## Problem
当前 overlay 主路径已经可用，仍有四类缺口：

1. **锚点稳定性**
   - 运行时只有静态 `line/column`，缺少随文档修改自动更新的锚点对象。
   - 异步流式返回期间，锚点解析逻辑仍然偏静态。

2. **接受写回语义**
   - `Tab` 接受目前直接调用 `view->insertText()`。
   - 事务边界可以更明确，后续扩展多步接受逻辑时也更稳。

3. **overlay 收尾体验**
   - 需要把渲染路径收敛到 baseline 驱动。
   - 需要把裁剪和超长文本显示行为收敛到确定规则。
   - 需要补 GUI 回归覆盖：行中间、文末、滚动离开再返回、Tab、Esc、焦点切换。

4. **发布材料缺口**
   - 仓库没有顶层 README。
   - 设置页缺少推荐 preset 与快捷键提示。
   - 现有截图在 `/tmp`，仓库内缺少可引用的截图资产。

## Questions and Answers
### Q1. MovingCursor 放在哪里最合适？
A1. `EditorSession` 持有运行时 `SuggestionAnchorTracker`，`GhostTextState` 只保存当前解析后的 `line/column` 与可见状态。这样渲染状态保持轻量，运行时锚点保持稳定。

### Q2. 锚点插入策略选什么？
A2. 选 `KTextEditor::MovingCursor::MoveOnInsert`。这样在锚点位置发生插入时，锚点会跟到新文本之后，更符合补全插入点跟随输入的语义。

### Q3. overlay 的垂直对齐怎么定义？
A3. 逐行使用 point-based `drawText()`，baseline 取 `lineTop + centeredTopOffset + fontMetrics.ascent()`。这样保留当前 line box 的垂直居中关系，同时用 baseline 控制文字落点。

### Q4. 超长建议如何截断？
A4. 纵向只绘制可见 viewport 内的行；横向对每一行使用 `QFontMetrics::elidedText(..., Qt::ElideRight, availableWidth)`，让显示规则稳定且可预测。

### Q5. 哪些行为进入自动化测试？
A5. 分成三层：
- `SuggestionAnchorTrackerTest`：验证锚点随文档编辑移动。
- `GhostTextOverlayWidgetRenderingTest`：验证行中、EOF、滚动离开/返回时的 overlay 输出。
- `EditorSessionIntegrationTest`：验证 SSE 请求驱动的 Tab、Esc、focus out 行为。

### Q6. 设置页与 README 这轮做到什么粒度？
A6. 这轮交付可直接使用的 README、仓库内截图、设置页 provider hint 与快捷键 hint。保留更精细的 GIF 或录屏到后续发布包装阶段。

## Design
### 1. Anchor Tracking
新增 `src/session/SuggestionAnchorTracker.{h,cpp}`：
- 持有 `std::unique_ptr<KTextEditor::MovingCursor>`。
- 提供：
  - `attach(Document*, Cursor, InsertBehavior)`
  - `clear()`
  - `isValid()`
  - `position()`
- `EditorSession` 在 `startRequest()` 创建 tracker。
- `onDeltaReceived()`、`onRequestFinished()`、`setSuppressed()`、`acceptSuggestion()` 先同步当前锚点，再更新 overlay。
- `GhostTextState` 新增 `anchorTracked` 布尔位，记录当前 suggestion 是否由活动 tracker 驱动。

### 2. Accept Path
`EditorSession::acceptSuggestion()` 更新为：
- 从 tracker 解析当前锚点。
- 仅当 view 当前 cursor 仍然等于解析后的锚点时接受。
- 用 `KTextEditor::Document::EditingTransaction` 包裹插入。
- 保持现有 `m_ignoreNextViewSignals` 节流逻辑。

### 3. Overlay Rendering
`GhostTextOverlayWidget` 收敛到以下规则：
- 统一获取 `effectiveTextFont()`。
- 用 baseline 点绘制逐行文字。
- 每行单独计算可用宽度并做 `elidedText()`。
- 对首行与后续行分别做 clip。
- 跳过不可见顶部行，遇到底部越界停止。
- 当 anchor 离开可视区域时，overlay 保留状态但当前帧不输出文字；滚动返回后可再次显示。

### 4. Tests
新增或扩展测试：
- `autotests/SuggestionAnchorTrackerTest.cpp`
- `autotests/GhostTextOverlayWidgetRenderingTest.cpp`
- `autotests/EditorSessionIntegrationTest.cpp`
- `autotests/KateAiConfigPageTest.cpp`

### 5. Release Prep
- 新增 `README.md`：构建、安装、开发态运行、Ollama/Copilot 配置、快捷键、评测工具入口。
- 新增 `docs/assets/` 截图并在 README 引用。
- `KateAiConfigPage` 增加：
  - provider recommendation label
  - shortcut hint label
  - 更明确的 KWallet / Copilot 状态文案

## Implementation Plan
1. 写失败测试：anchor tracker、overlay EOF/scroll、session Tab/Esc/focus、config page hints。
2. 运行定向测试，确认红灯。
3. 实现 `SuggestionAnchorTracker` 并接入 `EditorSession`。
4. 调整 overlay 为 baseline + elide + viewport clipping。
5. 调整设置页 hint 文案与状态文案。
6. 写 README，生成并纳入截图资产。
7. 跑全量 build + ctest，再补设计日志实现结果。

## Examples
### Example 1: 行中间请求
文档行为 `prefixSUFFIX`，cursor 在 `prefix` 后：
- 首行 ghost 从 `prefix|SUFFIX` 的 `|` 位置开始。
- 第二行开始从 text area 左边界绘制。
- 若首行太长，末尾显示省略号。

### Example 2: 文末多行建议
cursor 位于 EOF 空白行：
- overlay 在可见区域内绘制全部可见 ghost 行。
- 当 viewport 只容纳前两行时，只显示前两行。

### Example 3: 滚动离开后返回
anchor 初始在屏幕中部：
- 向上滚动让 anchor 离开 viewport，overlay 当前帧不输出文字。
- 滚动回原位置后，overlay 根据保留的 state 继续显示 suggestion。

## Trade-offs
- `MovingCursor` 提升锚点稳定性，同时把运行时复杂度集中在 session 层，渲染层保持简单。
- baseline 点绘制让字形落点更可控，同时需要手动处理每行裁剪与省略。
- README + 设置页 hint 会增加少量维护成本，同时显著降低首次试跑门槛。

## Implementation Results
- 新增运行时锚点模块：`src/session/SuggestionAnchorTracker.{h,cpp}`
  - `KTextEditor::MovingCursor::MoveOnInsert`
  - `EditorSession` 现在通过 tracker 同步 `GhostTextState::anchor`
  - `GhostTextState` 新增 `anchorTracked`
- `src/session/EditorSession.{h,cpp}`
  - 新增 `onDocumentTextChanged()`，让跨 view / programmatic document edits 可以刷新 overlay 锚点
  - `acceptSuggestion()` 现在先同步 tracker，再用 `KTextEditor::Document::EditingTransaction` 包裹 `view->insertText()`
  - overlay 同步统一收敛到 `applyStateToOverlay()`
- `src/render/GhostTextOverlayWidget.cpp`
  - 改为 baseline 点绘制
  - 增加逐行 clip
  - 增加 `QFontMetrics::elidedText()` 横向截断
  - 保持滚动离开后状态可恢复显示
- `src/settings/KateAiConfigPage.{h,cpp}`
  - 新增 provider hint label 与 shortcut hint label
  - 新增稳定 object name：`providerCombo`、`providerHintLabel`、`shortcutHintLabel`
  - KWallet / Copilot 状态文案更清晰
- 新增测试：
  - `autotests/SuggestionAnchorTrackerTest.cpp`
  - `autotests/EditorSessionIntegrationTest.cpp`
  - `autotests/KateAiConfigPageTest.cpp`
  - `autotests/GhostTextOverlayWidgetRenderingTest.cpp` 扩展 EOF / scroll 覆盖
- 新增发布材料：
  - `README.md`
  - `docs/assets/ghost-overlay-midline.png`
  - `docs/assets/ghost-overlay-eof.png`
- 验证：`cmake --build build -j 8 && ctest --test-dir build --output-on-failure && ls docs/assets`
  - Build succeeded
  - Tests: `12/12 passed`
  - Assets present: `ghost-overlay-eof.png`, `ghost-overlay-midline.png`
- 偏差记录：
  - design 里的 session 行为测试从“Tab/Esc/focus”扩展为包含“跨 view 编辑时锚点刷新”，因为这条路径最直接验证 MovingCursor 接入收益。
