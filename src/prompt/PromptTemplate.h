/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: PromptTemplate

    Builds system/user prompts for inline code completion.

    The plugin uses a Fill-in-the-Middle (FIM) representation where the model
    receives the text before the cursor as <|fim_prefix|> and the text after the
    cursor as <|fim_suffix|>. The model output is treated as the text inserted
    at the cursor position.

    Prompt templates are versioned via stable string IDs stored in
    CompletionSettings.
*/

#pragma once

#include <QString>
#include <QStringList>

namespace KateAiInlineCompletion
{

struct PromptContext {
    QString filePath;
    QString language;
    int cursorLine1 = -1;
    int cursorColumn1 = -1;
    QString prefix;
    QString suffix;
};

struct BuiltPrompt {
    QString systemPrompt;
    QString userPrompt;
    QStringList stopSequences;
};

class PromptTemplate
{
public:
    [[nodiscard]] static BuiltPrompt build(const QString &templateId, const PromptContext &ctx);

    [[nodiscard]] static QString sanitizeCompletion(const QString &raw);
};

} // namespace KateAiInlineCompletion
