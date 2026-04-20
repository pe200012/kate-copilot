Design Log #1 已创建，路径：docs/plans/2026-04-19-kate-ai-inline-completion-design.md。

内容摘要：
- 目标：Kate/KTextEditor 原生 AI 内联补全插件，使用 C++/Qt6/KF6，提供幽灵文字、SSE 流式更新、Tab 接受写回、与原生 completion popup 协同。
- 架构：Plugin 单例 + PluginView(每 MainWindow) + EditorSession(每 View) + GhostTextInlineNoteProvider + AbstractAIProvider(OpenAI-compatible/Ollama) + SSEParser + ContextExtractor + KWalletSecretStore + ConfigPage。
- 关键文件布局：src/plugin, src/session, src/render, src/network, src/context, src/settings, src/actions, autotests。
- 关键状态：SuggestionAnchor(view/cursor/generation), GhostTextState, CompletionRequest；每个 EditorSession 持有 debounce timer、generation、activeRequestId、note provider。
- 关键规则：建议只在 anchor.generation 与当前 generation 一致且 cursor 匹配时可见/可接受；view->isCompletionActive() 时进入 suppress；单 session 仅一个活动请求；事件过滤器挂到 view->editorWidget()。
- 默认设置：debounceMs=180, maxPrefixChars=12000, maxSuffixChars=3000, provider=openai-compatible；密钥存 KWallet，普通配置走文件配置。
- 路线图：Phase1 骨架与测试底座；Phase2 MVP（SSEParser/OpenAI provider/GhostTextInlineNoteProvider/EditorSession）；Phase3 Ollama + 多文件上下文 + 多行建议；Phase4 ConfigPage、集成测试、性能与文档。
- 当前待执行的最短实现链路：CMake/plugin.json/空插件 -> QtTest 底座 -> SSEParserTest + CompletionSettingsTest -> OpenAICompatibleProvider + SSEParser -> GhostTextInlineNoteProvider -> EditorSession(Tab/Esc/防抖/增量渲染)。