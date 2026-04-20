2026-04-20：完成 overlay 主路径稳定化与发布准备第一轮收尾。

实现：
- 新增 `src/session/SuggestionAnchorTracker.{h,cpp}`，用 `KTextEditor::MovingCursor::MoveOnInsert` 跟踪建议锚点。
- `EditorSession` 接入 tracker：
  - `onDocumentTextChanged()` 在跨 view / programmatic document edits 后同步 overlay 锚点
  - `applyStateToOverlay()` 统一同步 tracker + overlay state
  - `acceptSuggestion()` 用 `KTextEditor::Document::EditingTransaction` 包裹 `view->insertText()`
- `GhostTextState` 新增 `anchorTracked`，overlay 仅在 tracked anchor 有效时绘制。
- `GhostTextOverlayWidget` 改为 baseline 点绘制，增加逐行 clip 与 `QFontMetrics::elidedText()` 横向截断。
- 设置页新增发布向提示：
  - `providerHintLabel` 推荐 preset 文案
  - `shortcutHintLabel` 说明 `Tab` / `Esc` / popup suppress
  - KWallet / Copilot 状态文案更清晰
- 新测试：
  - `autotests/SuggestionAnchorTrackerTest.cpp`
  - `autotests/EditorSessionIntegrationTest.cpp`
  - `autotests/KateAiConfigPageTest.cpp`
  - 扩展 `autotests/GhostTextOverlayWidgetRenderingTest.cpp` 覆盖 EOF / scroll away+back
- 新文档与资产：
  - `README.md`
  - `docs/assets/ghost-overlay-midline.png`
  - `docs/assets/ghost-overlay-eof.png`
  - 新设计日志/实现计划：`docs/plans/2026-04-20-kate-ai-overlay-stabilization-and-release-prep-design.md`、`docs/plans/2026-04-20-kate-ai-overlay-stabilization-and-release-prep-implementation-plan.md`

验证：`cmake --build build -j 8 && ctest --test-dir build --output-on-failure && ls docs/assets` => 12/12 passed，两个 overlay 截图资产已生成。