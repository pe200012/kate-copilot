# Kate AI Inline Completion — Prompt 回归评测管线（HumanEval-Infilling + SAFIM）

Date: 2026-04-19

## Background

Kate AI 内联补全属于高频、低延迟的人机协作场景。补全质量需要同时满足：

1. **局部相关性**：补全与光标处上下文一致。
2. **可执行正确性**：补全在测试/编译/运行层面成立。
3. **输出一致性**：输出以“插入文本”形式出现，可直接写回文档。

本仓库已具备：OpenAI-compatible SSE 流式 provider、FIM prefix/suffix 上下文提取、幽灵文字渲染与 Tab 接受写回。

## Problem

当前 prompt 模板在真实编辑中出现“跑题”与“输出格式漂移”（解释、代码块、重复上下文）。需要一条可重复运行、可量化比较的回归评测管线，用于驱动 prompt 迭代。

## Questions and Answers

### Q1. 用哪些公开基准作为第一条回归线？

**A1.** 采用 HumanEval-Infilling 衡量 Python 代码的功能正确性（pass@k），采用 SAFIM 衡量多语言的结构补全与执行正确性（pass@1）。

### Q2. 如何把 Kate 的 FIM prompt 与基准任务对齐？

**A2.** 两个基准都提供 prefix 与 suffix 的“插入点”定义。评测侧统一构造：

- `prefix` → `<|fim_prefix|>` 内容
- `suffix` → `<|fim_suffix|>` 内容
- 插入点 → `<|fim_middle|>`

并将模型输出视为“待插入文本”。

### Q3. 如何安全执行基准中的不可信代码？

**A3.** 生成功能在宿主机侧调用 Ollama；执行评测在 Docker 容器内运行（HumanEval-Infilling 的 Python 测试、SAFIM 的 ExecEval）。

## Design

### 1. 评测工具目录

新增：`tools/eval/`

- `tools/eval/README.md`：环境准备、命令、输出解释。
- `tools/eval/kate_ai_eval.py`：统一 CLI（子命令 humaneval / safim）。
- `tools/eval/lib/ollama_openai_client.py`：OpenAI-compatible Chat Completions 客户端（stream=false）。
- `tools/eval/lib/prompt_templates.py`：可切换 prompt 模板（FIM v1/v2）。
- `tools/eval/lib/cache.py`：按 task_id 缓存生成结果，支持断点续跑。
- `tools/eval/lib/execeval.py`：ExecEval docker image 构建与 daemon 管理。

### 2. HumanEval-Infilling 回归

- 依赖：`openai/human-eval-infilling`
- 产物：`runs/humaneval_infilling/<benchmark>/<model>/<template>/samples.jsonl`
- 评测：生成后在 Docker 中执行 `evaluate_infilling_functional_correctness`，输出 pass@k 与 `_results.jsonl`。

### 3. SAFIM 回归

- 数据：HF dataset `gonglinyuan/safim`（configs: block/control/api/...）。
- 执行：对 `unit_tests` 非空样本调用 ExecEval 执行；对空测试样本执行语法/结构匹配。
- 产物：`runs/safim/<task>/<model>/<template>/outputs.jsonl` + `results.jsonl`。

### 4. Prompt 模板

提供两套模板，评测与插件端共用 ID 与语义：

- `fim_v1`：prefix/suffix + 语言标注
- `fim_v2`：在 v1 基础上增加 `<|fim_middle|>` 与游标元信息（file/language/cursor），并以“响应仅包含插入文本”为核心约束

模板设计参考：
- HumanEval-Infilling README（样本格式与评测入口）
- SAFIM README（ExecEval 与任务拆分）
- FIM prompting 资料（prefix/suffix/middle 语义）

## Implementation Plan

Phase A — Prompt 侧改造（仓库内）
1. 抽离 prompt 构造为独立模块（插件端），加入模板 ID。
2. Provider 支持 stop sequences（可选）。
3. 增加 prompt builder 单测。

Phase B — HumanEval-Infilling 工具链
1. `kate_ai_eval.py humaneval`：下载/安装依赖 → 生成 samples.jsonl → Docker 内评测。
2. 支持 `--benchmark single-line|multi-line|random-span|random-span-light`。
3. 支持 `--samples-per-task` 与缓存。

Phase C — SAFIM 工具链
1. `kate_ai_eval.py safim`：下载 ExecEval → build image → 启动 daemon → 生成 completions → 评测。
2. 支持 `--task block|control|api|block_v2|control_fixed`。
3. 支持 `--limit/--offset` 与缓存。

## Examples

### HumanEval-Infilling

```bash
python tools/eval/kate_ai_eval.py humaneval \
  --endpoint http://192.168.62.31:11434/v1/chat/completions \
  --model codestral:latest \
  --template fim_v2 \
  --benchmark single-line \
  --samples-per-task 1 \
  --out-dir tools/eval/runs
```

### SAFIM

```bash
python tools/eval/kate_ai_eval.py safim \
  --endpoint http://192.168.62.31:11434/v1/chat/completions \
  --model codestral:latest \
  --template fim_v2 \
  --task block \
  --limit 200 \
  --out-dir tools/eval/runs
```

## Trade-offs

1. **Docker 执行评测**
   - 收益：隔离不可信代码执行，复现实验环境。
   - 成本：首次构建 ExecEval image 耗时。

2. **samples-per-task 默认 1**
   - 收益：回归速度快，适合 prompt 迭代。
   - 成本：pass@10 等指标需要更高采样量。

3. **SAFIM 复刻评测逻辑（而非直接调用上游脚本）**
   - 收益：对 Python 版本与 tree-sitter 绑定更稳定。
   - 成本：需要保持与上游 evaluate.py 的语义一致。

## References

- HumanEval-Infilling: https://github.com/openai/human-eval-infilling
- SAFIM: https://github.com/gonglinyuan/safim
- ExecEval: https://github.com/ntunlp/ExecEval
- FIM prompting overview: https://vnavarro.dev/blog/fim
