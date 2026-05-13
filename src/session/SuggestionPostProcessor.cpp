/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: SuggestionPostProcessor
*/

#include "session/SuggestionPostProcessor.h"

#include "prompt/PromptTemplate.h"

#include <QStringList>
#include <QtGlobal>

namespace KateAiInlineCompletion
{

namespace
{
[[nodiscard]] QString normalizeNewlines(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

[[nodiscard]] bool isCloseTokenLine(const QString &line)
{
    const QString trimmed = line.trimmed();
    return trimmed == QStringLiteral("}") || trimmed == QStringLiteral(")") || trimmed == QStringLiteral("]")
        || trimmed == QStringLiteral("end");
}

[[nodiscard]] QString trimOneTrailingLine(QString text)
{
    if (text.endsWith(QLatin1Char('\n'))) {
        text.chop(1);
    }

    const int newline = text.lastIndexOf(QLatin1Char('\n'));
    if (newline < 0) {
        return {};
    }

    return text.left(newline);
}

[[nodiscard]] QString trimDuplicatedCloseLines(QString text, const QString &nextNonEmptyLine)
{
    const QString next = nextNonEmptyLine.trimmed();
    if (!isCloseTokenLine(next)) {
        return text;
    }

    while (!text.isEmpty()) {
        QString withoutFinalNewline = text;
        if (withoutFinalNewline.endsWith(QLatin1Char('\n'))) {
            withoutFinalNewline.chop(1);
        }

        const int newline = withoutFinalNewline.lastIndexOf(QLatin1Char('\n'));
        const QString lastLine = newline < 0 ? withoutFinalNewline : withoutFinalNewline.mid(newline + 1);
        if (lastLine.trimmed() != next || !isCloseTokenLine(lastLine)) {
            break;
        }

        text = trimOneTrailingLine(withoutFinalNewline);
    }

    return text;
}

[[nodiscard]] int suffixCoverageFor(const QString &insertText, const QString &currentLineSuffix)
{
    const QString suffix = currentLineSuffix;
    const int maxCoverage = qMin(insertText.size(), suffix.size());

    for (int coverage = maxCoverage; coverage > 0; --coverage) {
        if (insertText.endsWith(suffix.left(coverage))) {
            return coverage;
        }
    }

    return 0;
}
} // namespace

ProcessedSuggestion SuggestionPostProcessor::process(const QString &raw, const SuggestionProcessingContext &ctx)
{
    ProcessedSuggestion out;

    QString insertText = normalizeNewlines(PromptTemplate::sanitizeCompletion(raw));
    insertText = trimDuplicatedCloseLines(insertText, ctx.nextNonEmptyLine);

    if (insertText.trimmed().isEmpty()) {
        return out;
    }

    if (!ctx.nextNonEmptyLine.trimmed().isEmpty() && insertText.trimmed() == ctx.nextNonEmptyLine.trimmed()) {
        return out;
    }

    const int coverage = suffixCoverageFor(insertText, normalizeNewlines(ctx.currentLineSuffix));
    QString displayText = insertText;
    if (coverage > 0) {
        displayText.chop(coverage);
    }

    if (displayText.isEmpty()) {
        return out;
    }

    out.insertText = insertText;
    out.displayText = displayText;
    out.suffixCoverage = coverage;
    out.replaceRange = KTextEditor::Range(ctx.cursor, KTextEditor::Cursor(ctx.cursor.line(), ctx.cursor.column() + coverage));
    out.valid = ctx.cursor.isValid() && out.replaceRange.isValid();
    return out;
}

} // namespace KateAiInlineCompletion
