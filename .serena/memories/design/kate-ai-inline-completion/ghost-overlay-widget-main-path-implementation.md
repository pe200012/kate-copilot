2026-04-20：多行 ghost text 渲染主路径切换为 overlay（覆盖首行与后续行，支持任意光标位置）。

- 设计文档：`docs/plans/2026-04-20-kate-ai-ghost-overlay-renderer-design.md`
- 新模块：`src/render/GhostTextOverlayWidget.{h,cpp}`
  - 挂载到 `view->editorWidget()` 的透明子 widget
  - `setState(GhostTextState)` 驱动显示/隐藏与重绘
  - 绘制策略：
    - `visibleText.split('\n', KeepEmptyParts)`
    - 第 0 行从 anchor cursor 坐标开始绘制
    - 第 1..N 行从 `textAreaRect().left()` 对齐绘制，保留建议文本自身缩进
    - clip 到 `textAreaRect` 映射后的 editorWidget 坐标
    - 行高用 `cursorToCoordinate(line+1,0)-cursorToCoordinate(line,0)` 推导
- EditorSession 集成：`src/session/EditorSession.{h,cpp}`
  - 创建 `m_overlay = new GhostTextOverlayWidget(m_view, m_view->editorWidget())`
  - onDeltaReceived/onRequestFinished/clearSuggestion/setSuppressed 同步 overlay state
  - 监听 scroll + displayRangeChanged 触发 overlay->update()
  - ghost 渲染不再依赖 InlineNoteProvider
- 新测试：`autotests/GhostTextOverlayWidgetRenderingTest.cpp`
  - 验证 overlay 在行中间光标位置渲染首行，并向后续行区域绘制多行
- 构建验证：`cmake --build build -j 8 && ctest --test-dir build --output-on-failure` 9/9 passed。