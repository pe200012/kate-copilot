来源文件：针对开发-KDE-Kate-编辑器的-AI-辅助编程插件技术调研报告.md（2026-04-19 导出自 Gemini）。

目标：为 KDE Kate/KTextEditor 开发“Copilot 式”AI 内联补全插件，实现幽灵文字（Ghost Text）、流式生成、Tab 接受写回缓冲区、与 Kate 现有补全/LSP 协同。

关键技术决策
1) 语言与插件形态：核心采用 C++ + Qt6 + KDE Frameworks 6（KF6::TextEditor），按 KDE 插件规范用 kcoreaddons_add_plugin 构建 .so，并提供 plugin.json。
2) 幽灵文字渲染：采用 KTextEditor::InlineNoteProvider / InlineNote，在 paintInlineNote() 内用 QPainter 进行视图层像素渲染；文本仅存在于 View 绘制层，文档缓冲区保持零改动；颜色需适配主题；多行建议需要多注入点/行尾策略。
3) 触发与交互：监听 Document 的 textInserted/textChanged；以 QTimer singleShot 做防抖（建议 150–300ms，可配置）；View 上 installEventFilter 拦截按键：Tab 接受并将建议插入 Document；Esc 隐藏；可扩展 Ctrl+Right 逐词接受；必要时配合焦点链控制。
4) 网络与流式：QNetworkAccessManager + HTTP SSE（Accept: text/event-stream）；QNetworkReply::readyRead 增量读取；实现健壮 SSE 分片解析（按 \n\n 切分 data: 载荷，QJsonDocument 解析 token），每次增量更新触发重绘。
5) 后端 Provider 抽象：AbstractAIProvider + OpenAI/Anthropic（云端）与 Ollama（本地）实现；密钥用 KWallet 加密存取；本地推理需旧请求终止（reply->abort + deleteLater）以释放 GPU；可选前缀缓存（Trie）减少延迟与重复请求。
6) 上下文策略：FIM（prefix/suffix）切片 + 文档元数据（highlightingMode、缩进等）；用 token/字符预算做窗口控制；项目级增强：打开标签页上下文、Project 插件 API、CTags/LSP 符号定义片段，按“Context File … / Current File … / <|fim_prefix|> … <|fim_suffix|>”结构化组装 Prompt。
7) 与 LSP 协同：插件保持独立渲染权；通过 KTextEditor::CodeCompletionInterface 侦测传统补全弹窗状态；策略：语义补全优先、AI 幽灵文字在弹窗激活时进入 suppress/暂停模式。

路线图（四阶段）
- Phase1 环境与骨架：kdesrc-build 沙盒编译 kate/ktexteditor；工程骨架、设置 UI、KWallet。
- Phase2 MVP：防抖 + SSE 管道 + InlineNote 幽灵文字 + eventFilter(Tab 接受) 打通端到端。
- Phase3 拓展：Ollama、本地取消、ContextExtractor、多文件/项目上下文、与 LSP 互斥协同、RTL/动态换行适配测试。
- Phase4 产品化：Valgrind 生命周期审计；动作/快捷键用 KActionCollection；CI(Flatpak)；文档与 Alpha 发布。