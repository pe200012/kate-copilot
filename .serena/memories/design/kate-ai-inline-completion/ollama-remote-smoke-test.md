已使用远端 Ollama（OpenAI-compatible API）做网络联通与流式解析验证。

测试目标
- 服务器：http://192.168.62.31:11434
- OpenAI-compatible endpoint：/v1/chat/completions

验证步骤
1) 列出模型：
   curl -sS http://192.168.62.31:11434/v1/models

2) 通过本仓库新增 CLI 工具调用真实 Provider（OpenAICompatibleProvider）：
   ./build/bin/kateaiinlinecompletion_ollama_smoke_test \
     --endpoint http://192.168.62.31:11434/v1/chat/completions \
     --model codestral:latest \
     --prompt "Write a single-line Python program that prints hello." \
     --max-tokens 80 \
     --timeout-ms 120000

实际输出（节选）
- 流式 token 输出：print("hello")
- 结束汇总：endpoint/model/chars 正常打印

结论
- SSE framing（SSEParser）+ OpenAICompatibleProvider 的 choices[0].delta.content 解析路径与 [DONE] 结束标记在该 Ollama 服务端可用。