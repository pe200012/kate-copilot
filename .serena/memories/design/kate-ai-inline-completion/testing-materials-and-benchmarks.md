用于系统测试 Kate AI 内联补全（实现正确性 + Prompt 质量 + 真实项目相关性）的公开资料与基准。

A. Prompt/FIM 模板资料
- FIM 机制与三标记（prefix/suffix/middle）：https://vnavarro.dev/blog/fim
- Prompt template 汇总（不同模型的 FIM token 与传统指令模板）：https://chabicht.github.io/code-intelligence/PROMPT-TEMPLATES.html
- Tabby completion prompt_template 与模型连接文档（含 FIM 示例）：https://tabby.tabbyml.com/docs/administration/model
- Neovim FIM 补全插件示例（包含 stop sequences 与多模型 prompt_format）：https://github.com/heyfixit/shrimply-suggest.nvim

B. 离线基准（可用于评估“相关性/可执行正确性”）
- HumanEval-Infilling（FIM 评测 harness，pass@k + 单行/多行/随机 span）：https://github.com/openai/human-eval-infilling
- SAFIM（Syntax-Aware FIM，多语言，配套生成与评测脚本，leaderboard）：
  - 主页/榜单：https://safimbenchmark.com/
  - 代码：https://github.com/gonglinyuan/safim
  - 论文：https://arxiv.org/abs/2403.04814
- Real-FIM-Eval（真实 GitHub commit 生成的 FIM 任务，多语言）：
  - 数据集：https://huggingface.co/datasets/gonglinyuan/real_fim_eval
  - 论文：https://arxiv.org/abs/2506.00204
- RepoBench（仓库级自动补全，检索+补全+端到端 pipeline）：
  - 代码：https://github.com/Leolty/repobench
  - 论文：https://arxiv.org/abs/2306.03091
- CrossCodeEval（跨文件依赖的补全基准，静态分析生成样本）：
  - 主页：https://crosscodeeval.github.io/
  - 代码：https://github.com/amazon-science/cceval
  - 论文：https://arxiv.org/abs/2310.11248
- ExecRepoBench（可执行的仓库级补全，带单元测试验证）：
  - 主页：https://execrepobench.github.io/
  - 论文：https://arxiv.org/abs/2412.11990
- Codev-Bench（开发者视角、仓库级评测框架，含 prompts 与执行环境）：https://github.com/LingmaTongyi/Codev-Bench
- C3-Bench（可控代码补全，带指令约束的 completion 评测）：https://arxiv.org/abs/2601.15879

C. 编辑器侧交互测试样例（借鉴）
- VS Code inline completion API 的官方测试计划条目（覆盖：suggest widget 激活时行为、显式触发、悬浮等）：https://github.com/microsoft/vscode/issues/124979
- VS Code inline completion 示例扩展代码：
  https://github.com/microsoft/vscode-extension-samples/blob/main/inline-completions/src/extension.ts

D. 流式协议验证资料
- OpenAI 流式返回提取 delta.content 的 cookbook：https://developers.openai.com/cookbook/examples/how_to_stream_completions/
- OpenAI Chat Completions streaming events 参考：https://developers.openai.com/api/reference/resources/chat/subresources/completions/streaming-events/
- Ollama OpenAI compatibility（含 /v1/chat/completions streaming、/v1/completions 的 suffix 字段支持）：https://docs.ollama.com/api/openai-compatibility

落地建议（与本仓库实现对接）
- 以 HumanEval-Infilling + SAFIM 作为 prompt 模板回归测试的第一组基准（体量适中，harness 成熟）。
- 以 RepoBench/CrossCodeEval/ExecRepoBench/Codev-Bench 作为 Phase 3 的跨文件上下文与 RAG 回归基准。