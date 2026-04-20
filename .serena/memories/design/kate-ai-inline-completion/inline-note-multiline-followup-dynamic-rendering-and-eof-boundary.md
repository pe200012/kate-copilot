2026-04-20 跟进调试：用户反馈“按 Tab 前只看到首行”。

调查结果：
- 新增 GUI 集成测试 `autotests/GhostTextInlineNoteProviderRenderingTest.cpp`。
- 该测试直接创建 `KTextEditor::View` 与真实 `GhostTextInlineNoteProvider`，在运行中把 state 从单行更新为多行，抓图验证渲染结果。
- 调试截图：
  - `/tmp/kate-ai-inline-note-rendering/ghost_inline_single_line.png`
  - `/tmp/kate-ai-inline-note-rendering/ghost_inline_multiline.png`
- 实证：当锚点下方已经存在真实行时，多行 InlineNote 会在运行中正确扩展，第二、第三行 ghost 能显示。
- 结论：之前怀疑的“动态更新只刷新首行”并非当前根因。
- 根因进一步收敛到已知 API 边界：InlineNote 的后续 ghost 行需要已有真实行承载。超出文档末尾的行没有 host line 时，视图只能显示首行 suggestion；Tab 接受仍会插入完整多行文本。
- 验证：`cmake --build build -j 8 && ctest --test-dir build --output-on-failure` 7/7 passed。