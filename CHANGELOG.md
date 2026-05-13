# Changelog

## v0.1.0 - Initial public release

### Added
- Native KDE Kate / KTextEditor plugin for AI inline completion.
- Overlay-based ghost text rendering attached to `view->editorWidget()`.
- Streaming SSE completions for OpenAI-compatible endpoints.
- Ollama support through `/v1/chat/completions`.
- GitHub Copilot OAuth device flow with KWallet token storage.
- Copilot Codex completions provider.
- Full accept, next-word accept, next-line accept, dismiss, and manual trigger commands.
- Kate-native actions for configurable shortcuts.
- Prompt templates `fim_v1`, `fim_v2`, and `fim_v3`, with `fim_v3` as the default.
- HumanEval-Infilling and SAFIM evaluation harness under `tools/eval/`.
- Gentoo live ebuild under `packaging/gentoo/app-editors/kate-copilot/`.
- LGPL-2.0-or-later license file.

### Fixed
- Multi-line ghost text rendering uses a transparent overlay path.
- Ghost overlay tracks view font changes, scroll, resize, and display range changes.
- Suggestion anchor tracking uses `KTextEditor::MovingCursor`.
- Accept operations use `KTextEditor::Document::EditingTransaction`.
- Copilot endpoint is fixed for the Copilot provider.
- Copilot HTTP failures include categorized messages.

### Known limitations
- Overlay rendering is view-local and visual-only; it does not reserve virtual document space.
- Perfect Copilot-style multi-line layout requires KTextEditor virtual block / virtual line support.
- Copilot API behavior can change upstream.
