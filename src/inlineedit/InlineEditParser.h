/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditParser

    Parses structured JSON inline edit responses into bounded multi-range suggestions.
*/

#pragma once

#include "inlineedit/InlineEdit.h"

namespace KTextEditor
{
class Document;
}

namespace KateAiInlineCompletion
{

struct InlineEditParserOptions {
    int maxNewTextChars = 8000;
    int maxTotalNewTextChars = 16000;
    int maxEdits = 4;
    bool allowDeletion = false;
    KTextEditor::Range expectedRange = KTextEditor::Range::invalid();
};

class InlineEditParser final
{
public:
    [[nodiscard]] static InlineEditSuggestion parse(const QString &response,
                                                    KTextEditor::Document *document,
                                                    const InlineEditParserOptions &options = {});
};

} // namespace KateAiInlineCompletion
