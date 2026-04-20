已落地：Prompt 模板版本化 + 离线回归评测工具（HumanEval-Infilling + SAFIM）。

A. 插件端 Prompt 模板
- 新增模块：`src/prompt/PromptTemplate.{h,cpp}`
  - 模板 ID：`fim_v1`（legacy），`fim_v2`（包含 file/cursor 元信息 + `<|fim_middle|>`）
  - 输出：`BuiltPrompt { systemPrompt, userPrompt, stopSequences }`
  - `sanitizeCompletion()`：支持 `<|fim_middle|>`/`<|fim_suffix|>` 截断 + code fence 提取
- `CompletionRequest` 扩展：`QStringList stopSequences`（`src/network/AbstractAIProvider.h`）
- Provider 支持 stop：`src/network/OpenAICompatibleProvider.cpp` payload 添加 `stop`
- EditorSession：流式增量写入 `m_rawSuggestionText`，实时对 `visibleText` 做 sanitize（避免 fences/标记污染写回）
- API key 规则：当 `settings.provider == ollama` 时 endpoint 可为空 key；其余 provider 对非 localhost endpoint 提示需要 key
- 配置页：`src/settings/KateAiConfigPage.*` 增加 Prompt group 与 Template 下拉框，读写 `settings.promptTemplate`
- Settings：`CompletionSettings` 已含 `promptTemplate` 字段与校验/序列化（`PromptTemplate` key）
- 单测新增：`autotests/PromptTemplateTest.cpp`；现有 CompletionSettingsTest 已覆盖 promptTemplate roundtrip

B. 离线回归评测工具（tools/eval）
- 文档：`tools/eval/README.md`
- 依赖：`tools/eval/requirements.txt`（requests/tqdm/fire/numpy/datasets）
- CLI：`tools/eval/kate_ai_eval.py`
  - `humaneval` 子命令：
    - 自动 clone `openai/human-eval-infilling` 到 `tools/eval/_deps`
    - 生成 `samples.jsonl`（可断点续跑，按 task_id 计数补齐）
    - `--enable-unsafe-exec` 时自动 patch execution.py 取消 exec 注释，并运行 upstream `human_eval_infilling.evaluate_functional_correctness`
  - `safim` 子命令：
    - 使用 HF dataset `gonglinyuan/safim`
    - 自动 clone `ntunlp/ExecEval`，build docker image `exec-eval:1.0`
    - 仅在存在 unit_tests 样本时启动 ExecEval daemon；对空单测样本走 syntax_match
    - 输出 `outputs.jsonl` 与 `results.jsonl`

C. 构建验证
- `cmake --build build`
- `ctest --test-dir build --output-on-failure`：4/4 passed（新增 prompt_template_test）
- Python：`python3 -m py_compile tools/eval/kate_ai_eval.py tools/eval/lib/*.py` 通过

D. 关键运行命令（与远端 Ollama 对接）
- HumanEval-Infilling：
  `python tools/eval/kate_ai_eval.py humaneval --endpoint http://192.168.62.31:11434/v1/chat/completions --model codestral:latest --template fim_v2 --benchmark single-line --samples-per-task 1 --out-dir tools/eval/runs --enable-unsafe-exec`
- SAFIM：
  `python tools/eval/kate_ai_eval.py safim --endpoint http://192.168.62.31:11434/v1/chat/completions --model codestral:latest --template fim_v2 --task block --limit 200 --out-dir tools/eval/runs`
