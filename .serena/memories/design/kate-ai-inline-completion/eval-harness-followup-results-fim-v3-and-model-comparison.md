后续迭代：新增 fim_v3 模板，并完成容器化评测的多模型实跑（远端 Ollama）。

A. PromptTemplate fim_v3（插件端 + eval harness 对齐）
- CompletionSettings：新增 `kPromptTemplateFimV3="fim_v3"`，默认值切到 fim_v3
- ConfigPage：Prompt Template 下拉增加 FIM v3
- `src/prompt/PromptTemplate.cpp`：
  - system prompt 加入 file/language/cursor 元信息与“插入文本”约束
  - fim_v3 user prompt 以纯 FIM 形式输出：`<|fim_prefix|>{prefix}<|fim_suffix|>{suffix}<|fim_middle|>`（无额外换行插入）
  - stopSequences 增加 "```"，减少 code fence 干扰
- 单测：`PromptTemplateTest` 增加 fim_v3 case；`CompletionSettingsTest` 更新默认模板断言

B. HumanEval-Infilling 评测保持容器执行
- 评测 Docker image：`kate-ai-humaneval-infilling-eval:1.0`

C. eval harness 增强：支持 OpenAI `/v1/completions` 文本补全接口
- `tools/eval/lib/ollama_openai_client.py` 新增：
  - `TextCompletionParams` + `text_completion()`（解析 `choices[0].text`）
- `tools/eval/lib/humaneval_infilling.py`：
  - endpoint 以 `/v1/completions` 结尾时使用 `text_completion(prompt=prefix, suffix=suffix)`
  - `benchmark == single-line` 时对 completion 做“一行 + 结尾换行”归一化
- `tools/eval/lib/safim.py`：
  - endpoint 以 `/v1/completions` 结尾时使用 `text_completion(prompt=prefix, suffix=suffix)`

D. 关键发现
- Codestral 的原生 FIM token 体系与 `<|fim_prefix|>` 不一致（公开资料显示 Codestral 采用 `[SUFFIX]`/`[PREFIX]`/`[MIDDLE]`，以及专用 FIM endpoint），因此 `<|fim_prefix|>` 在 codestral 上 infilling 分数偏低。
- Ollama `/v1/completions` + `suffix`（insert）并非所有模型都支持：`qwen3-coder-q4:latest` 返回 “does not support insert”。

E. 实跑结果（远端 Ollama，容器执行评测）
1) HumanEval-Infilling single-line（pass@1）
- codestral:latest + chat endpoint + fim_v3 + limit 50：pass@1 = 0.0
- codestral:latest + completions endpoint（suffix insert）+ limit 50：pass@1 = 0.02
- qwen3-coder-q4:latest + chat endpoint + fim_v3：
  - limit 50：pass@1 = 0.7
  - 全量 1033 tasks：pass@1 = 0.750242013552759
  - 产物：`tools/eval/runs_chat/humaneval_infilling/single-line/qwen3-coder-q4_latest/fim_v3/samples.jsonl(_results)`

2) SAFIM block（pass@1，limit 50）
- codestral:latest + chat endpoint + fim_v3：Pass@1 = 6.0%
- codestral:latest + completions endpoint（suffix insert）：Pass@1 = 0.0%（输出出现大量乱码/跨语言漂移）
- qwen3-coder-q4:latest + chat endpoint + fim_v3：Pass@1 = 2.0%

F. 对插件使用的建议（基于评测）
- 以 `/v1/chat/completions` + `qwen3-coder-q4:latest` + fim_v3 作为 Kate 内联补全的高质量默认组合。
- 若继续主推 codestral，需要实现 codestral 专用的 FIM token/endpoint 路径（例如 `/v1/completions` insert 或 `[SUFFIX]/[PREFIX]` prompt format），并在插件端 provider 层做策略切换。