实现记录：GitHub Copilot OAuth + Codex Completions（B 路线）+ 多行像素下推 ghost（Phase 1）。

关键代码路径
- Settings
  - `src/settings/CompletionSettings.{h,cpp}`
    - provider 新增：`github-copilot-codex`
    - 新字段：`copilotClientId`（默认 `Iv1.b507a08c87ecfe98`）、`copilotNwo`（默认 `github/copilot.vim`）
    - provider 默认 endpoint：`https://copilot-proxy.githubusercontent.com/v1/engines/copilot-codex/completions`
- Secrets
  - `src/settings/KWalletSecretStore.{h,cpp}`
    - 新 entry：`githubOAuthToken`
    - API：`hasGitHubOAuthToken/readGitHubOAuthToken/writeGitHubOAuthToken/removeGitHubOAuthToken`
- Auth
  - `src/auth/CopilotAuthManager.{h,cpp}`
    - 交换：`GET https://api.github.com/copilot_internal/v2/token`
    - 缓存：`token + expiresAtMs + endpoints.api`，带 5min safety margin
    - 并发合并：单次 exchange 服务多个 acquire
    - Header 常量（Phase 1 固定）：
      - `User-Agent: GitHubCopilotChat/0.26.7`
      - `Editor-Version: vscode/1.99.3`
      - `Editor-Plugin-Version: copilot-chat/0.26.7`
      - `Copilot-Integration-Id: vscode-chat`
- Provider
  - `src/network/CopilotCodexProvider.{h,cpp}`
    - SSE parse：`choices[0].text` → `deltaReceived()`
    - `[DONE]` 结束
    - 401/403：`invalidateSessionToken()` 后触发一次 refresh+retry
  - `src/network/AbstractAIProvider.h`
    - `CompletionRequest` 扩展：`prompt/suffix/nwo/extra/topP/n`
- Prompt
  - `src/prompt/CopilotCodexPromptBuilder.{h,cpp}`
    - prompt：`(#|//) Path: <filePath>\n` + prefix
    - `next_indent`：按 `tab-width` 计算行首缩进列宽
    - `languageId`：lowercase + 归一化
- Multi-line Ghost
  - `src/render/GhostTextPushDownOverlay.{h,cpp}`
    - editorWidget 快照渲染（RenderFlags 仅 DrawWindowBackground，避免绘 children）
    - anchor 行以下像素整体下推
    - 鼠标/滚轮触发 `interactionOccurred()`，EditorSession 清理建议后转发事件
- Session/Integration
  - `src/session/EditorSession.{h,cpp}`
    - provider 多态：OpenAICompatibleProvider / CopilotCodexProvider
    - Copilot 请求 payload：`prompt/suffix/extra{language,next_indent,trim_by_indentation}`
    - overlay 与 inline note 同步状态
- UI
  - `src/settings/KateAiConfigPage.{h,cpp}`
    - Provider 下拉新增 Copilot
    - Copilot OAuth UI：device code flow（Sign in / polling / Sign out）
    - Copilot Client ID 与 NWO 可配置

验证
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`：4/4 passed

联调
- `QT_PLUGIN_PATH=$PWD/build/bin kate`
- Settings → AI Completion → Backend 选择 `GitHub Copilot (OAuth)`，Endpoint 填 completions URL，完成 Sign in 后输入触发建议。