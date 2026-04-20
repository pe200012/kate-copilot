Phase 2（MVP 主链路骨架）已落地到可编译、可链接、测试通过的状态。

新增模块
- SSE framing：src/network/SSEParser.*（静态库 kateaiinlinecompletion_sse）
- Provider：src/network/AbstractAIProvider.h、src/network/OpenAICompatibleProvider.*
  - SSE data 解析 choices[0].delta.content
  - 识别 [DONE] / finish_reason
  - cancel(requestId) -> reply->abort
- 渲染：src/render/GhostTextInlineNoteProvider.*
  - 通过 KTextEditor::InlineNoteProvider 在 anchor 行列渲染单行幽灵文字
  - 颜色使用 Text/Base 混合得到低对比度
- 会话：src/session/GhostTextState.h、src/session/EditorSession.*
  - 监听 View::textInserted / cursorPositionChanged / selectionChanged / focusOut
  - QTimer 单次防抖触发请求
  - eventFilter 拦截 Tab/Esc
  - generation 机制避免陈旧流写回
- PluginView：src/plugin/KateAiInlineCompletionPluginView.*
  - 在 viewChanged 时 ensureSession(view)
  - 复用 QNetworkAccessManager 与 KWalletSecretStore

构建与测试
- 变更 install namespace：kf6/ktexteditor（与 /usr/lib64/qt6/plugins/kf6/ktexteditor 对齐）
- ctest：3/3 passed（CompletionSettingsTest、SSEParserTest、appstreamtest）

当前能力范围
- 单行幽灵文字渲染
- SSE token 流式增量更新
- Tab 接受写回 Document::insertText
- completion popup 可见时按设置 suppress

待继续
- ContextExtractor 与更精细的 prompt 预算/策略
- 多行渲染与逐词接受
- Provider 抽象下的 Ollama 专用策略（可复用 OpenAI-compatible endpoint）