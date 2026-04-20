2026-04-20：新增部分接受（partial accept）能力。

交互：
- Tab：接受整段建议
- Esc：清理建议
- Ctrl+Alt+Shift+Right：接受 next word（按词/符号块）
- Ctrl+Alt+Shift+L：接受 next line（包含换行）

实现：
- `src/session/EditorSession.{h,cpp}`
  - public: `acceptNextWord()` / `acceptNextLine()`
  - 内部 `acceptPartial(chunk)`：更新 `m_acceptedFromSuggestion` 与 `m_state.visibleText`，用 `KTextEditor::Document::EditingTransaction` 包裹 `view->insertText(chunk)`，随后 `applyStateToOverlay()` 同步 MovingCursor 锚点与 overlay
  - `onDeltaReceived()`：`full = PromptTemplate::sanitizeCompletion(m_rawSuggestionText)`，用 `full.mid(m_acceptedFromSuggestion.size())` 生成当前 ghost，可在流式增量中保持已接受前缀不回流
  - 分段规则：空白段、word 段（含尾随空白）、符号段（含尾随空白）、换行段
- 文档/提示：
  - `src/settings/KateAiConfigPage.cpp` shortcut hint 更新
  - `README.md` shortcut 列表更新
- 测试：
  - `autotests/EditorSessionIntegrationTest.cpp` 新增 `acceptNextWordAcceptsWordChunk()` 与 `acceptNextLineAcceptsLineChunk()`

设计文档：`docs/plans/2026-04-20-kate-ai-partial-accept-design.md`
验证：`ctest --test-dir build --output-on-failure` => 12/12 passed。