/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: SuggestionPostProcessor

    Normalizes provider completions and computes single-line replace ranges.
*/

#pragma once

#include <KTextEditor/Cursor>
#include <KTextEditor/Range>

#include <QString>

namespace KateAiInlineCompletion
{

struct SuggestionProcessingContext {
    KTextEditor::Cursor cursor;
    QString currentLineSuffix;
    QString nextNonEmptyLine;
};

struct ProcessedSuggestion {
    QString displayText;
    QString insertText;
    KTextEditor::Range replaceRange = KTextEditor::Range::invalid();
    int suffixCoverage = 0;
    bool valid = false;
};

class SuggestionPostProcessor
{
public:
    [[nodiscard]] static ProcessedSuggestion process(const QString &raw, const SuggestionProcessingContext &ctx);
};

} // namespace KateAiInlineCompletion
