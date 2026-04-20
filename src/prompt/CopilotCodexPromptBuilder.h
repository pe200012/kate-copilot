/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CopilotCodexPromptBuilder

    Builds prompt + suffix inputs for GitHub Copilot Codex completions.

    The provider consumes a plain `prompt` plus a separate `suffix` field.
    This aligns with Copilot's insert-mode (FIM) completion protocol.
*/

#pragma once

#include "prompt/PromptTemplate.h"

#include <KTextEditor/Cursor>

#include <QString>

namespace KTextEditor
{
class Document;
}

namespace KateAiInlineCompletion
{

struct CopilotCodexPrompt {
    QString prompt;
    QString suffix;
    QString languageId;
    int nextIndent = 0;
};

class CopilotCodexPromptBuilder
{
public:
    [[nodiscard]] static CopilotCodexPrompt build(const PromptContext &ctx,
                                                  KTextEditor::Document *doc,
                                                  const KTextEditor::Cursor &cursor);

private:
    [[nodiscard]] static QString commentPrefixForLanguage(const QString &language);
    [[nodiscard]] static int computeNextIndent(KTextEditor::Document *doc, const KTextEditor::Cursor &cursor);
    [[nodiscard]] static QString normalizeLanguageId(QString language);
};

} // namespace KateAiInlineCompletion
