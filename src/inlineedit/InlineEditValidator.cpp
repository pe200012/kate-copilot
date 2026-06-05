/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditValidator
*/

#include "inlineedit/InlineEditValidator.h"

#include <KTextEditor/Document>

#include <QtGlobal>

#include <algorithm>
#include <limits>
#include <utility>

namespace KateAiInlineCompletion
{
namespace
{
[[nodiscard]] int boundedSize(qsizetype value)
{
    return static_cast<int>(qMin<qsizetype>(value, std::numeric_limits<int>::max()));
}

[[nodiscard]] bool cursorInDocument(KTextEditor::Document *document, const KTextEditor::Cursor &cursor)
{
    if (!document || !cursor.isValid() || cursor.line() < 0 || cursor.line() >= document->lines()) {
        return false;
    }

    const int lineLength = boundedSize(document->line(cursor.line()).size());
    return cursor.column() >= 0 && cursor.column() <= lineLength;
}

[[nodiscard]] bool rangeInDocument(KTextEditor::Document *document, const KTextEditor::Range &range)
{
    return range.isValid() && cursorInDocument(document, range.start()) && cursorInDocument(document, range.end()) && range.start() <= range.end();
}

[[nodiscard]] bool rangeStartsBefore(const ProposedEdit &a, const ProposedEdit &b)
{
    if (a.range.start().line() != b.range.start().line()) {
        return a.range.start().line() < b.range.start().line();
    }

    if (a.range.start().column() != b.range.start().column()) {
        return a.range.start().column() < b.range.start().column();
    }

    if (a.range.end().line() != b.range.end().line()) {
        return a.range.end().line() < b.range.end().line();
    }

    return a.range.end().column() < b.range.end().column();
}

[[nodiscard]] bool isInsertion(const ProposedEdit &edit)
{
    return edit.range.start() == edit.range.end();
}

[[nodiscard]] QString normalizedNewlines(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

[[nodiscard]] InlineEditValidationResult failure(QString message)
{
    InlineEditValidationResult result;
    result.message = std::move(message);
    return result;
}
} // namespace

InlineEditValidationResult InlineEditValidator::validate(KTextEditor::Document *document,
                                                         const InlineEditSuggestion &suggestion,
                                                         const InlineEditValidationOptions &options)
{
    if (!document) {
        return failure(QStringLiteral("Inline edit document is unavailable"));
    }

    if (options.expectedDocumentRevision >= 0 && document->revision() != options.expectedDocumentRevision) {
        return failure(QStringLiteral("Inline edit target document changed"));
    }

    const int maxEdits = qMax(1, options.maxEdits);
    if (!suggestion.valid || suggestion.edits.isEmpty()) {
        return failure(QStringLiteral("Inline edit suggestion has no edits"));
    }

    if (suggestion.edits.size() > maxEdits) {
        return failure(QStringLiteral("Inline edit suggestion has too many edits"));
    }

    const int maxNewTextChars = qMax(0, options.maxNewTextChars);
    const int maxTotalNewTextChars = qMax(0, options.maxTotalNewTextChars);
    int totalNewTextChars = 0;
    QVector<ProposedEdit> edits;
    edits.reserve(suggestion.edits.size());

    for (const ProposedEdit &edit : suggestion.edits) {
        if (!rangeInDocument(document, edit.range)) {
            return failure(QStringLiteral("Inline edit range is outside the document"));
        }

        if (edit.newText.size() > maxNewTextChars) {
            return failure(QStringLiteral("Inline edit replacement text is too large"));
        }

        totalNewTextChars += boundedSize(edit.newText.size());
        if (totalNewTextChars > maxTotalNewTextChars) {
            return failure(QStringLiteral("Inline edit replacement text total is too large"));
        }

        if (isInsertion(edit) && edit.newText.isEmpty()) {
            return failure(QStringLiteral("Inline edit empty insertion is invalid"));
        }

        if (!options.allowDeletion && edit.newText.isEmpty()) {
            return failure(QStringLiteral("Inline edit deletion is disabled"));
        }

        if (normalizedNewlines(document->text(edit.range)) == normalizedNewlines(edit.newText)) {
            return failure(QStringLiteral("Inline edit does not change the document"));
        }

        edits.push_back(edit);
    }

    std::sort(edits.begin(), edits.end(), rangeStartsBefore);

    for (int i = 1; i < edits.size(); ++i) {
        const ProposedEdit &previous = edits.at(i - 1);
        const ProposedEdit &current = edits.at(i);

        if (previous.range == current.range) {
            return failure(QStringLiteral("Inline edit ranges are duplicated"));
        }

        if (isInsertion(previous) && isInsertion(current) && previous.range.start() == current.range.start()) {
            return failure(QStringLiteral("Inline edit duplicate insertions are ambiguous"));
        }

        if (previous.range.start() == current.range.start()) {
            return failure(QStringLiteral("Inline edit ranges share a start position"));
        }

        if (previous.range.end() > current.range.start()) {
            return failure(QStringLiteral("Inline edit ranges overlap"));
        }
    }

    InlineEditValidationResult result;
    result.ok = true;
    result.edits = std::move(edits);
    return result;
}

} // namespace KateAiInlineCompletion
