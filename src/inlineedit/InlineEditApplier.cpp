/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditApplier
*/

#include "inlineedit/InlineEditApplier.h"

#include <KTextEditor/Document>

#include <algorithm>
#include <utility>

namespace KateAiInlineCompletion
{
namespace
{
[[nodiscard]] bool rangeStartsAfter(const ProposedEdit &a, const ProposedEdit &b)
{
    if (a.range.start().line() != b.range.start().line()) {
        return a.range.start().line() > b.range.start().line();
    }

    return a.range.start().column() > b.range.start().column();
}

[[nodiscard]] InlineEditApplyResult failure(QString message)
{
    InlineEditApplyResult result;
    result.message = std::move(message);
    return result;
}
} // namespace

InlineEditApplyResult InlineEditApplier::apply(KTextEditor::Document *document,
                                               const InlineEditSuggestion &suggestion,
                                               const InlineEditValidationOptions &options)
{
    const InlineEditValidationResult validation = InlineEditValidator::validate(document, suggestion, options);
    if (!validation.ok) {
        return failure(validation.message);
    }

    QVector<ProposedEdit> edits = validation.edits;
    std::sort(edits.begin(), edits.end(), rangeStartsAfter);

    KTextEditor::Document::EditingTransaction transaction(document);
    for (const ProposedEdit &edit : std::as_const(edits)) {
        if (!document->replaceText(edit.range, edit.newText)) {
            return failure(QStringLiteral("Failed to apply inline edit"));
        }
    }

    InlineEditApplyResult result;
    result.ok = true;
    return result;
}

} // namespace KateAiInlineCompletion
