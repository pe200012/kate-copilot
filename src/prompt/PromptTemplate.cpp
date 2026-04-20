/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: PromptTemplate
*/

#include "prompt/PromptTemplate.h"

#include "settings/CompletionSettings.h"

namespace KateAiInlineCompletion
{

namespace
{
[[nodiscard]] QString buildSystemPrompt(const PromptContext &ctx)
{
    QString meta;

    if (!ctx.filePath.trimmed().isEmpty()) {
        meta += QStringLiteral("File: %1. ").arg(ctx.filePath.trimmed());
    }

    if (!ctx.language.trimmed().isEmpty()) {
        meta += QStringLiteral("Language: %1. ").arg(ctx.language.trimmed());
    }

    if (ctx.cursorLine1 > 0 && ctx.cursorColumn1 > 0) {
        meta += QStringLiteral("Cursor: line %1, column %2. ").arg(ctx.cursorLine1).arg(ctx.cursorColumn1);
    }

    return QStringLiteral(
               "Role: code completion engine. "
               "Task: generate the exact text inserted at the cursor between the provided FIM prefix and suffix. "
               "Output: inserted text only, as plain text. "
               "Formatting: indentation and newlines follow surrounding code. ")
        + meta;
}

[[nodiscard]] QString buildUserPromptFimV1(const PromptContext &ctx)
{
    return QStringLiteral("// Language: %1\n<|fim_prefix|>\n%2\n<|fim_suffix|>\n%3")
        .arg(ctx.language, ctx.prefix, ctx.suffix);
}

[[nodiscard]] QString buildUserPromptFimV2(const PromptContext &ctx)
{
    const QString fileLine = ctx.filePath.trimmed().isEmpty() ? QString() : QStringLiteral("// File: %1\n").arg(ctx.filePath);

    const QString cursorLine = (ctx.cursorLine1 > 0 && ctx.cursorColumn1 > 0)
        ? QStringLiteral("// Cursor: line %1, column %2\n").arg(ctx.cursorLine1).arg(ctx.cursorColumn1)
        : QString();

    return QStringLiteral("%1// Language: %2\n%3<|fim_prefix|>\n%4\n<|fim_suffix|>\n%5\n<|fim_middle|>")
        .arg(fileLine, ctx.language, cursorLine, ctx.prefix, ctx.suffix);
}

[[nodiscard]] QString buildUserPromptFimV3(const PromptContext &ctx)
{
    return QStringLiteral("<|fim_prefix|>") + ctx.prefix + QStringLiteral("<|fim_suffix|>") + ctx.suffix
        + QStringLiteral("<|fim_middle|>");
}

[[nodiscard]] QString extractFirstFencedBlock(const QString &raw)
{
    const QString fence = QStringLiteral("```");
    const int start = raw.indexOf(fence);
    if (start < 0) {
        return raw;
    }

    int afterFence = raw.indexOf(QLatin1Char('\n'), start + fence.size());
    if (afterFence < 0) {
        afterFence = start + fence.size();
    } else {
        ++afterFence;
    }

    const int end = raw.indexOf(fence, afterFence);
    if (end < 0) {
        return raw.mid(afterFence);
    }

    return raw.mid(afterFence, end - afterFence);
}

} // namespace

BuiltPrompt PromptTemplate::build(const QString &templateId, const PromptContext &ctx)
{
    BuiltPrompt out;
    out.systemPrompt = buildSystemPrompt(ctx);

    const QString id = templateId.trimmed().toLower();

    if (id == QString::fromLatin1(CompletionSettings::kPromptTemplateFimV1)) {
        out.userPrompt = buildUserPromptFimV1(ctx);
        return out;
    }

    out.stopSequences = {
        QStringLiteral("<|fim_prefix|>"),
        QStringLiteral("<|fim_suffix|>"),
        QStringLiteral("<|fim_middle|>"),
        QStringLiteral("```")
    };

    if (id == QString::fromLatin1(CompletionSettings::kPromptTemplateFimV2)) {
        out.userPrompt = buildUserPromptFimV2(ctx);
        return out;
    }

    out.userPrompt = buildUserPromptFimV3(ctx);
    return out;
}

QString PromptTemplate::sanitizeCompletion(const QString &raw)
{
    QString out = raw;

    const QString middle = QStringLiteral("<|fim_middle|>");
    const int middlePos = out.indexOf(middle);
    if (middlePos >= 0) {
        out = out.mid(middlePos + middle.size());
    }

    const QString suffix = QStringLiteral("<|fim_suffix|>");
    const int suffixPos = out.indexOf(suffix);
    if (suffixPos >= 0) {
        out = out.left(suffixPos);
    }

    out = extractFirstFencedBlock(out);
    return out;
}

} // namespace KateAiInlineCompletion
