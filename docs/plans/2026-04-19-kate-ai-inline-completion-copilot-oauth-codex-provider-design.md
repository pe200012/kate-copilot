# Design Log: Kate AI Inline Completion — GitHub Copilot OAuth + Codex Completions (Phase 1)

Date: 2026-04-19

## Background
Kate AI Inline Completion 已具备：
- EditorSession（防抖触发、SSE 增量、Tab 接受写回、Esc 清理）
- OpenAI-compatible provider（/v1/chat/completions，解析 `choices[0].delta.content`）
- GhostTextInlineNoteProvider（InlineNote 单行幽灵文字）

本设计在现有架构上新增 **GitHub Copilot (OAuth)** Provider，Phase 1 目标聚焦 **B 路线**：
- 调用 `https://copilot-proxy.githubusercontent.com/v1/engines/copilot-codex/completions`（prompt + suffix，SSE `choices[0].text`）
- OAuth 使用 GitHub device code flow
- GitHub OAuth token 持久化到 KWallet
- Copilot session token（~25min）内存缓存 + 自动刷新
- 多行 ghost 以“像素下推”方式呈现（虚拟行观感）

## Problem
1) Kate 插件当前仅支持 OpenAI-compatible chat completions。
2) InlineNoteProvider 的高度以单行行高裁剪，多行虚拟行观感需要额外渲染层。
3) Copilot 需要 OAuth 登录与 token 交换：
   - Device flow 拿到 GitHub OAuth token
   - `GET https://api.github.com/copilot_internal/v2/token` 换取短期 Copilot session token

## Questions and Answers
### Q1. Phase 1 API 选择
- A: `api.*.githubcopilot.com/chat/completions`
- B: `copilot-proxy.githubusercontent.com/.../completions`

**Answer:** 选择 B。

### Q2. OAuth client_id 策略
- 方案 1：内置默认 client_id（Copilot 常用 device-flow client_id），设置页允许覆盖
- 方案 2：要求用户填写自建 OAuth App 的 client_id

**Answer:** 选择方案 1。

### Q3. 多行补全与呈现
- 多行补全：启用 `extra.trim_by_indentation`，动态计算 `extra.next_indent`
- 呈现：像素下推 overlay

**Answer:** 采用多行补全 + 像素下推。

## Design

### 1) Settings / Secrets
#### CompletionSettings 变更
- 新 provider：`github-copilot-codex`
- 新字段：
  - `copilotClientId: QString`（默认 `Iv1.b507a08c87ecfe98`，允许覆盖）
  - `copilotNwo: QString`（默认 `github/copilot.vim`）

Provider-specific endpoint 默认值：
- Copilot Codex completions：`https://copilot-proxy.githubusercontent.com/v1/engines/copilot-codex/completions`

#### KWalletSecretStore 变更
新增条目：
- `githubOAuthToken`

API:
- `hasGitHubOAuthToken()/readGitHubOAuthToken()/writeGitHubOAuthToken()/removeGitHubOAuthToken()`

Copilot session token 仅做内存缓存，不写入 KWallet。

### 2) Auth
新增模块：`src/auth/CopilotAuthManager.{h,cpp}`（QObject）

职责：
- 读取 GitHub OAuth token（KWallet）
- `GET https://api.github.com/copilot_internal/v2/token` 换取 Copilot session token
- 缓存：`token + expiresAtMs + baseApiUrl`，并带 5 分钟安全余量
- 并发合并：同一时间只发起一个 exchange，请求队列共享结果

交换请求 headers（固定常量，Phase 1）：
- `User-Agent: GitHubCopilotChat/0.26.7`
- `Editor-Version: vscode/1.99.3`
- `Editor-Plugin-Version: copilot-chat/0.26.7`
- `Copilot-Integration-Id: vscode-chat`
- `Accept: application/json`
- `Authorization: Bearer <github_oauth_token>`

### 3) Provider (B)
新增模块：`src/network/CopilotCodexProvider.{h,cpp}`，实现 `AbstractAIProvider`。

请求：
- POST `request.endpoint`（默认 copilot-proxy）
- `Authorization: Bearer <copilot_session_token>`
- `Accept: text/event-stream`
- JSON payload：
  - `prompt: QString`（prefix + meta）
  - `suffix: QString`
  - `model: QString`（沿用 settings.model）
  - `stream: true`
  - `max_tokens / temperature / top_p / n`
  - `stop: QStringList`（Phase 1 默认包含 ```）
  - `nwo: settings.copilotNwo`
  - `extra: { language, next_indent, trim_by_indentation: true }`

响应：
- SSE framing 复用 `SSEParser`
- 解析 `choices[0].text`，逐段 `deltaReceived()`
- 识别 `data: [DONE]`，触发 `requestFinished()`

### 4) Prompt construction (for Copilot Codex)
新增：`src/prompt/CopilotCodexPromptBuilder.{h,cpp}`

输入：
- `filePath, language, prefix, suffix, cursor`

输出：
- `prompt`：
  - 以 `# Path: <filePath>` 作为首行（Copilot 常见格式）
  - 后续拼接 prefix
- `suffix`：原样使用 extractSuffix

`next_indent` 计算：
- 取光标所在行 `doc->line(cursor.line())`
- 统计行首 whitespace 的“虚拟列宽”
  - `tab-width` 来自 `doc->configValue("tab-width")`

### 5) 多行 Ghost 呈现：像素下推 overlay
新增：`src/render/GhostTextPushDownOverlay.{h,cpp}`（QWidget，parent= `view->editorWidget()`）

渲染策略：
- InlineNote 继续负责首行（当前实现保持不变）
- overlay 负责第 2..N 行：
  - 计算 anchor 行在 overlay 坐标系的 `ySplit`（anchor 行底部 y）
  - `shiftPx = (lineCount-1) * lineHeightPx`
  - 维护 editorWidget 的快照 pixmap（render 时禁用 DrawChildren，避免递归绘制 overlay）
  - 绘制：
    1) 直接绘制快照上半部分 `[0, ySplit)`
    2) 在 `[ySplit, ySplit+shiftPx)` 绘制背景
    3) 绘制快照下半部分到 `ySplit+shiftPx`
    4) 在空白区逐行绘制 ghost 行文本

交互一致性：
- EditorSession 的 eventFilter 捕获 `MouseButtonPress / Wheel / Drag` 等输入时先 `bumpGeneration()` 清理建议，让点击与滚动落在真实布局上。

### 6) Config UI
扩展 `KateAiConfigPage`：
- Provider 下拉增加 `GitHub Copilot (OAuth)`
- Credentials 区域按 provider 切换显示：
  - OpenAI-compatible/Ollama：保留现有 API key 存储
  - Copilot：新增 OAuth 区块
    - 显示 KWallet 状态 + GitHub token 是否存在
    - `Sign in`：启动 device flow
    - 显示 `verification_uri` + `user_code` + `Open`/`Copy` 按钮
    - 轮询 `access_token` 成功后写入 KWallet
    - `Sign out`：移除 GitHub token

Device flow endpoint：
- `POST https://github.com/login/device/code`
- `POST https://github.com/login/oauth/access_token`

## Implementation Plan
Phase 1（可独立合并、可运行）：
1) Settings + KWallet
   - CompletionSettings 增加 provider + copilotClientId + copilotNwo
   - KWalletSecretStore 增加 GitHub OAuth token API
   - autotests：settings 序列化/校验覆盖新字段

2) Auth
   - CopilotAuthManager：token exchange + cache + refresh margin + 并发合并
   - autotests：token response JSON 解析与 expires 处理

3) Provider
   - CopilotCodexProvider：SSE -> `choices[0].text` 增量
   - EditorSession：AbstractAIProvider 多态、按 settings.provider 选择 provider

4) 多行 ghost overlay
   - GhostTextPushDownOverlay：像素下推渲染 + 快照节流
   - EditorSession：状态变更时同步 overlay

5) Config UI
   - Copilot 登录/登出 UI 与 device flow 轮询

6) 手工联调
   - 在 Kate GUI：登录 -> 输入触发 -> 多行幽灵文字出现 -> Tab 写回 -> 鼠标点击清理

## Examples
- OAuth 登录：点击 Sign in 后显示 `verification_uri + user_code`，用户在浏览器输入 code，插件轮询成功后显示“已登录”。
- 多行 ghost：suggestion 含 `\n` 时，首行行内显示，后续行以虚拟行方式出现在光标下方，后文像素下推。

## Trade-offs
- 像素下推属于纯渲染效果，文档内容保持原状，鼠标点击在视觉位移状态下不具备一致命中语义；事件触发清理后命中恢复一致。
- prompt 构造先采用 Copilot 常见 `# Path:` 风格，后续可按语言体系扩展为 `// Path:` 等。
- 内置 device flow client_id 有合规风险，设置支持覆盖以便切换到自建 OAuth App。

## Implementation Results (2026-04-19)
### 已实现模块
- Settings
  - `src/settings/CompletionSettings.{h,cpp}`：新增 provider `github-copilot-codex`，新增 `copilotClientId`/`copilotNwo`，Copilot provider 下 endpoint 固定为 Codex completions URL。
- Secrets
  - `src/settings/KWalletSecretStore.{h,cpp}`：新增 `githubOAuthToken` 存取 API。
- Auth
  - `src/auth/CopilotAuthManager.{h,cpp}`：`copilot_internal/v2/token` 交换、缓存、并发合并、失效重试入口。
- Provider
  - `src/network/CopilotCodexProvider.{h,cpp}`：SSE 解析 `choices[0].text` 增量输出，401/403 触发一次 refresh+retry；HTTP 错误回显 status code + 错误详情。
  - `src/network/AbstractAIProvider.h`：扩展 `CompletionRequest` 支持 `prompt/suffix/nwo/extra/topP/n`。
- Prompt
  - `src/prompt/CopilotCodexPromptBuilder.{h,cpp}`：生成 `prompt+suffix`，计算 `next_indent`，标准化 language id。
- Multi-line Ghost Render
  - `src/render/GhostTextPushDownOverlay.{h,cpp}`：像素下推渲染第 2..N 行；鼠标/滚轮触发清理建议后转发事件。
- Session integration
  - `src/session/EditorSession.{h,cpp}`：provider 多态选择、Copilot 请求构造、overlay 状态同步。
- UI
  - `src/settings/KateAiConfigPage.{h,cpp}`：新增 Provider 选项、Copilot OAuth 登录/登出、device flow 轮询、Client ID/NWO 配置；Copilot provider 下 Endpoint 字段置灰并自动填充默认 URL。

### 构建与测试
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`：4/4 passed

### 手工联调指令
1) 启动 Kate（加载本地插件）
   - `QT_PLUGIN_PATH=$PWD/build/bin kate`
2) Settings → AI Completion
   - Backend：GitHub Copilot (OAuth)
   - Endpoint：`https://copilot-proxy.githubusercontent.com/v1/engines/copilot-codex/completions`
   - Sign in：按提示打开 `verification_uri` 并输入 `user_code`
3) 编辑任意源码文件，等待防抖触发
   - 单行 ghost 走 InlineNote
   - 多行 ghost 走像素下推 overlay
   - `Tab` 接受写回
   - `Esc` 清理建议
