跟进修复（2026-04-20）：
- Config UI：Copilot provider 选中时 Endpoint 字段置灰（`KateAiConfigPage::updateCredentialsUi()`），并在 `slotProviderChanged()` 强制填入默认 Codex completions URL。
- Settings 校验：`CompletionSettings::validated()` 在 provider 为 `github-copilot-codex` 时强制 endpoint 为 `https://copilot-proxy.githubusercontent.com/v1/engines/copilot-codex/completions`，避免误配导致 HTTP 401。
- 错误信息：`CopilotCodexProvider` 在 HTTP>=400 时回显 `HTTP <code>: <detail>`，detail 优先解析 JSON `error.message/message`，否则回退到响应 body 或 Qt errorString。
- 新增单测：`CompletionSettingsTest::copilotForcesEndpoint()` 覆盖 endpoint 强制行为。
- 验证：`cmake --build build` + `ctest` 4/4 passed。