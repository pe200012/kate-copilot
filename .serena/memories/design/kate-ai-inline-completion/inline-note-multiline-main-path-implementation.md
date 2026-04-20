2026-04-20：多行 ghost text 主路径收敛到 InlineNote。

- 新设计文档：`docs/plans/2026-04-20-kate-ai-inline-note-multiline-design.md`
- 用户选择：后续 ghost 行按“建议文本自身缩进”显示。
- 实现：
  - `GhostTextInlineNoteProvider` 将 `visibleText` 按 `Qt::KeepEmptyParts` 拆分。
  - 首行 note 仍挂在 `anchor.column`。
  - 后续行 note 挂在后续真实行的列 `0`，缩进保留在 note 文本里。
  - `inlineNoteSize()` / `paintInlineNote()` 基于 `note.position().line()` 选择对应逻辑行文本。
  - `setState()` 对旧/新受影响行并集逐行发 `inlineNotesChanged(line)`，保证多行渲染刷新。
- `EditorSession`：
  - 不再创建 `GhostTextPushDownOverlay`
  - 不再同步 overlay state
  - InlineNote 成为唯一运行时渲染路径
- 新测试：`autotests/GhostTextInlineNoteProviderTest.cpp`
  - 验证多行 suggestion 映射到 anchor 行 + 后续真实行
  - 验证扩展到多行时会刷新整段受影响行
- 验证：`cmake --build build -j 8 && ctest --test-dir build --output-on-failure` 6/6 passed。