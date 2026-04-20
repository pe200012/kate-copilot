评测工具已容器化并完成一次实跑（远端 Ollama + codestral:latest）。

A. 工具链改动（容器化）
1) HumanEval-Infilling 评测在 Docker 内执行
- 新增：`tools/eval/docker/humaneval-infilling/Dockerfile`
  - 镜像：`kate-ai-humaneval-infilling-eval:1.0`（python:3.11-slim + numpy/tqdm/fire）
- 新增：`tools/eval/lib/docker_humaneval_infilling.py`
  - `docker run` 隔离参数：`--network none --cap-drop ALL --security-opt no-new-privileges --pids-limit 512 --memory 4g --cpus 2`
- `tools/eval/lib/humaneval_infilling.py`
  - `evaluate()` 改为调用容器执行 upstream `python -m human_eval_infilling.evaluate_functional_correctness`
  - `enable_unsafe_exec()` 做两项 patch：
    - execution.py：启用 `exec(check_program, exec_globals)`
    - evaluation.py：启用 subset 评测模式（移除“覆盖全部 task”断言），支持 `--limit-tasks` 快速回归

2) SAFIM
- 单元测试样本的代码执行由 ExecEval docker daemon 承担。

B. 路径规范化
- 新增：`tools/eval/lib/path_utils.py`：`slugify_segment()`
  - 将模型 id（含 `:`）规范化为目录名，兼容 Docker volume 语法
- HumanEval/SAFIM 输出目录均改用 `slugify_segment(cfg.model)`

C. 实跑结果（小规模回归）
1) HumanEval-Infilling（subset 5 tasks）
- 命令：
  `python tools/eval/kate_ai_eval.py humaneval --endpoint http://192.168.62.31:11434/v1/chat/completions --model codestral:latest --template fim_v2 --benchmark single-line --samples-per-task 1 --limit-tasks 5 --out-dir tools/eval/runs --enable-unsafe-exec --k 1 --n-workers 2 --eval-timeout-s 6`
- 结果：`pass@1 = 0.0`
- 产物：
  - `tools/eval/runs/humaneval_infilling/single-line/codestral_latest/fim_v2/samples.jsonl`
  - `tools/eval/runs/humaneval_infilling/single-line/codestral_latest/fim_v2/samples.jsonl_results.jsonl`

2) SAFIM block（limit 20）
- 命令：
  `python tools/eval/kate_ai_eval.py safim --endpoint http://192.168.62.31:11434/v1/chat/completions --model codestral:latest --template fim_v2 --task block --limit 20 --out-dir tools/eval/runs --execeval-port 5000 --execeval-workers 2`
- 结果：`Pass 4 / Total 20`，`Pass@1 = 20.0%`
- 产物：
  - `tools/eval/runs/safim/block/codestral_latest/fim_v2/outputs.jsonl`
  - `tools/eval/runs/safim/block/codestral_latest/fim_v2/results.jsonl`
