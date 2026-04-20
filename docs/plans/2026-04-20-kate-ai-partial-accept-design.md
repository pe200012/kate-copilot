# Kate AI Partial Acceptance Design (Next Word / Next Line)

- Date: 2026-04-20
- Status: Implemented
- Scope: Allow accepting part of the current ghost suggestion without clearing the remaining text.

## Background
当前插件支持：
- ghost overlay 预览（`GhostTextOverlayWidget`）
- `Tab` 接受整段建议
- `Esc` 清理建议
- `MovingCursor` 锚点跟踪（`SuggestionAnchorTracker`）

GitHub Copilot 与 VS Code inline suggestion 生态里存在“按词/按行部分接受”的交互（相关快捷键与命令在 GitHub 文档与 VS Code 的 inlineSuggest 命令体系中有明确对应）：
- GitHub Docs — Keyboard shortcuts: https://docs.github.com/en/copilot/reference/keyboard-shortcuts

## Problem
整段接受会导致长建议在光标处一次性写入，用户希望按小步接受：
- 接受下一个“词/符号块”
- 接受下一行（包含换行）
- 保持剩余 ghost 继续显示
- 流式生成时持续可用

## Questions and Answers
### Q1. 部分接受的锚点如何保持正确？
A1. 锚点由 `KTextEditor::MovingCursor` 维护。每次插入 chunk 后，通过 `syncAnchorFromTracker()` 刷新 anchor，并用 overlay 重绘。

### Q2. 如何保证已接受部分不会在后续 delta 中重复出现？
A2. 引入 `m_acceptedFromSuggestion` 作为“已接受前缀”。每次 `sanitizeCompletion(m_rawSuggestionText)` 产出 `full` 后，用 `full.mid(m_acceptedFromSuggestion.size())` 更新 `visibleText`。

### Q3. next-word 的边界规则是什么？
A3. 采用可重复触发的分段规则：
- `\n` 单独作为一个 chunk
- 行内空白（不含 `\n`）作为一个 chunk
- `[A-Za-z0-9_]` 连续段作为一个 chunk，并吸收其后的行内空白
- 其他符号以 1 个字符为 chunk，并吸收其后的行内空白

### Q4. next-line 的边界规则是什么？
A4. 接受到下一个 `\n` 并包含该换行；当不存在换行时接受剩余全部。

### Q5. 快捷键选择策略是什么？
A5. 插件提供固定快捷键，优先选择与 Kate 常规编辑移动行为隔离的组合：
- `Ctrl+Alt+Shift+Right`：accept next word
- `Ctrl+Alt+Shift+L`：accept next line

## Design
### EditorSession API
- 新增 public commands：
  - `EditorSession::acceptNextWord()`
  - `EditorSession::acceptNextLine()`

### State
- `EditorSession` 新增：`QString m_acceptedFromSuggestion`
- `onDeltaReceived()`：
  - `m_rawSuggestionText += delta`
  - `full = sanitizeCompletion(m_rawSuggestionText)`
  - `visibleText = full.mid(m_acceptedFromSuggestion.size())`

### Acceptance algorithm
- `acceptPartial(chunk)`：
  - 预检查：anchorTracked、anchor 同步、cursor 匹配、chunk 为 visibleText 前缀
  - `m_acceptedFromSuggestion += chunk`
  - `m_state.visibleText = m_state.visibleText.mid(chunk.size())`
  - `EditingTransaction` + `view->insertText(chunk)`
  - `applyStateToOverlay()`

### Event handling
当 ghost 可见时拦截按键：
- `Tab`：整段接受
- `Esc`：清理
- `Ctrl+Alt+Shift+Right`：next word
- `Ctrl+Alt+Shift+L`：next line

## Implementation Results
- 代码：
  - `src/session/EditorSession.{h,cpp}`：新增部分接受命令与逻辑
  - `src/settings/KateAiConfigPage.cpp`：快捷键提示更新
  - `README.md`：快捷键文档更新
- 新增/更新测试：
  - `autotests/EditorSessionIntegrationTest.cpp`：覆盖 next-word / next-line 的行为与锚点移动
- 验证：`ctest --test-dir build --output-on-failure` 显示 12/12 passed（包含集成测试）。

## Trade-offs
- next-word 分段规则属于 editor-agnostic 的简化版本；它优先保证“可重复触发、插入位置稳定、不会越界”。
- next-line 接受包含换行，光标进入下一行开头，保留 Kate 自动缩进语义给后续输入或下一次部分接受。
