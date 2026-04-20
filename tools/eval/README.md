# Kate AI Inline Completion — Prompt 回归评测（HumanEval-Infilling + SAFIM）

本目录提供一套可重复运行的评测管线，用于对比不同 prompt 模板在“FIM 补全”任务上的质量。

- **HumanEval-Infilling**：Python 任务，执行测试得到 pass@k。
- **SAFIM**：多语言任务，执行测试（ExecEval）与结构匹配得到 pass@1。

## 环境准备

1. 创建虚拟环境（推荐）

```bash
cd tools/eval
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -U pip
python -m pip install -r requirements.txt
```

2. 确认 Docker 可用（HumanEval-Infilling 与 SAFIM 评测通过容器执行）

```bash
docker --version
```

## 运行 HumanEval-Infilling

> 开启 `--enable-unsafe-exec` 后，评测在 Docker 容器内执行，并自动构建镜像 `kate-ai-humaneval-infilling-eval:1.0`。

```bash
python kate_ai_eval.py humaneval \
  --endpoint http://192.168.62.31:11434/v1/chat/completions \
  --model codestral:latest \
  --template fim_v2 \
  --benchmark single-line \
  --samples-per-task 1 \
  --out-dir runs \
  --enable-unsafe-exec
```

输出：
- `runs/humaneval_infilling/<benchmark>/<model>/<template>/samples.jsonl`
- `runs/humaneval_infilling/<benchmark>/<model>/<template>/samples.jsonl_results.jsonl`

## 运行 SAFIM

SAFIM 的执行评测通过 ExecEval daemon 完成，本工具会自动：
- clone ExecEval
- build `exec-eval:1.0` docker image
- 启动 daemon 容器并等待就绪

```bash
python kate_ai_eval.py safim \
  --endpoint http://192.168.62.31:11434/v1/chat/completions \
  --model codestral:latest \
  --template fim_v2 \
  --task block \
  --limit 200 \
  --out-dir runs
```

输出：
- `runs/safim/<task>/<model>/<template>/outputs.jsonl`
- `runs/safim/<task>/<model>/<template>/results.jsonl`

## Prompt 模板

- `fim_v1`：语言标注 + prefix/suffix
- `fim_v2`：增加 file/cursor 元信息与 `<|fim_middle|>`

模板实现位于：`tools/eval/lib/prompt_templates.py`（与插件端 `src/prompt/PromptTemplate.*` 对齐）。
