"""Module: prompt_templates

Prompt builders for offline evaluation.

The functions mirror the plugin-side behavior in `src/prompt/PromptTemplate.*`.
They intentionally keep the templates small and stable so that benchmark results
remain comparable across iterations.
"""

from __future__ import annotations

from dataclasses import dataclass


TEMPLATE_FIM_V1 = "fim_v1"
TEMPLATE_FIM_V2 = "fim_v2"
TEMPLATE_FIM_V3 = "fim_v3"


@dataclass(frozen=True)
class PromptContext:
    file_path: str
    language: str
    cursor_line1: int
    cursor_column1: int
    prefix: str
    suffix: str


@dataclass(frozen=True)
class BuiltPrompt:
    system_prompt: str
    user_prompt: str
    stop_sequences: list[str]


def _system_prompt(ctx: PromptContext) -> str:
    meta = ""
    if ctx.file_path.strip():
        meta += f"File: {ctx.file_path.strip()}. "
    if ctx.language.strip():
        meta += f"Language: {ctx.language.strip()}. "
    if ctx.cursor_line1 > 0 and ctx.cursor_column1 > 0:
        meta += f"Cursor: line {ctx.cursor_line1}, column {ctx.cursor_column1}. "

    return (
        "Role: code completion engine. "
        "Task: generate the exact text inserted at the cursor between the provided FIM prefix and suffix. "
        "Output: inserted text only, as plain text. "
        "Formatting: indentation and newlines follow surrounding code. "
        + meta
    )


def _user_prompt_fim_v1(ctx: PromptContext) -> str:
    return (
        f"// Language: {ctx.language}\n"
        "<|fim_prefix|>\n"
        f"{ctx.prefix}\n"
        "<|fim_suffix|>\n"
        f"{ctx.suffix}"
    )


def _user_prompt_fim_v2(ctx: PromptContext) -> str:
    file_line = f"// File: {ctx.file_path}\n" if ctx.file_path.strip() else ""
    cursor_line = (
        f"// Cursor: line {ctx.cursor_line1}, column {ctx.cursor_column1}\n"
        if ctx.cursor_line1 > 0 and ctx.cursor_column1 > 0
        else ""
    )

    return (
        f"{file_line}// Language: {ctx.language}\n"
        f"{cursor_line}"
        "<|fim_prefix|>\n"
        f"{ctx.prefix}\n"
        "<|fim_suffix|>\n"
        f"{ctx.suffix}\n"
        "<|fim_middle|>"
    )


def _user_prompt_fim_v3(ctx: PromptContext) -> str:
    return f"<|fim_prefix|>{ctx.prefix}<|fim_suffix|>{ctx.suffix}<|fim_middle|>"


def build(template_id: str, ctx: PromptContext) -> BuiltPrompt:
    template_id = (template_id or "").strip().lower()

    if template_id == TEMPLATE_FIM_V1:
        return BuiltPrompt(
            system_prompt=_system_prompt(ctx),
            user_prompt=_user_prompt_fim_v1(ctx),
            stop_sequences=[],
        )

    stop = ["<|fim_prefix|>", "<|fim_suffix|>", "<|fim_middle|>", "```"]

    if template_id == TEMPLATE_FIM_V2:
        return BuiltPrompt(
            system_prompt=_system_prompt(ctx),
            user_prompt=_user_prompt_fim_v2(ctx),
            stop_sequences=stop,
        )

    return BuiltPrompt(
        system_prompt=_system_prompt(ctx),
        user_prompt=_user_prompt_fim_v3(ctx),
        stop_sequences=stop,
    )


def _extract_first_fenced_block(text: str) -> str:
    fence = "```"
    start = text.find(fence)
    if start < 0:
        return text

    after_fence = text.find("\n", start + len(fence))
    if after_fence < 0:
        after_fence = start + len(fence)
    else:
        after_fence += 1

    end = text.find(fence, after_fence)
    if end < 0:
        return text[after_fence:]

    return text[after_fence:end]


def sanitize_completion(raw: str) -> str:
    out = raw or ""

    middle = "<|fim_middle|>"
    middle_pos = out.find(middle)
    if middle_pos >= 0:
        out = out[middle_pos + len(middle) :]

    suffix = "<|fim_suffix|>"
    suffix_pos = out.find(suffix)
    if suffix_pos >= 0:
        out = out[:suffix_pos]

    out = _extract_first_fenced_block(out)
    return out
