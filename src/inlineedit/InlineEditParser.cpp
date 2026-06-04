/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditParser
*/

#include "inlineedit/InlineEditParser.h"

#include <KTextEditor/Document>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringView>
#include <QtGlobal>

#include <limits>

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

[[nodiscard]] int boundedSize(qsizetype value)
{
    return static_cast<int>(qMin<qsizetype>(value, std::numeric_limits<int>::max()));
}

[[nodiscard]] QString stripJsonFence(const QString &response)
{
    QString trimmed = response.trimmed();
    if (!trimmed.startsWith(QStringLiteral("```"))) {
        return trimmed;
    }

    const int firstNewline = boundedSize(trimmed.indexOf(QLatin1Char('\n')));
    if (firstNewline < 0) {
        return trimmed;
    }

    const int endFence = boundedSize(trimmed.lastIndexOf(QStringLiteral("```")));
    if (endFence <= firstNewline) {
        return trimmed;
    }

    return trimmed.mid(firstNewline + 1, endFence - firstNewline - 1).trimmed();
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
    if (!range.isValid()) {
        return false;
    }

    if (!cursorInDocument(document, range.start()) || !cursorInDocument(document, range.end())) {
        return false;
    }

    return range.start() <= range.end();
}

[[nodiscard]] QString displayTextFor(const QString &newText)
{
    const QStringList lines = newText.split(QLatin1Char('\n'));
    if (lines.size() <= 3) {
        return newText;
    }

    return lines.mid(0, 3).join(QLatin1Char('\n')) + QStringLiteral("\n…");
}

[[nodiscard]] int intValue(const QJsonObject &object, const QString &key)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return std::numeric_limits<int>::min();
    }
    return value.toInt(std::numeric_limits<int>::min());
}
} // namespace

InlineEditSuggestion InlineEditParser::parse(const QString &response, KTextEditor::Document *document, const InlineEditParserOptions &options)
{
    InlineEditSuggestion out;
    out.rawResponse = response;
    out.source = QStringLiteral("manual");

    if (!document) {
        return out;
    }

    const QString jsonText = stripJsonFence(response);
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        return out;
    }

    const QJsonArray edits = parsed.object().value(QStringLiteral("edits")).toArray();
    if (edits.size() != 1 || !edits.at(0).isObject()) {
        return out;
    }

    const int maxNewTextChars = qMax(0, options.maxNewTextChars);
    const QJsonObject edit = edits.at(0).toObject();
    const int startLine = intValue(edit, QStringLiteral("startLine"));
    const int startColumn = intValue(edit, QStringLiteral("startColumn"));
    const int endLine = intValue(edit, QStringLiteral("endLine"));
    const int endColumn = intValue(edit, QStringLiteral("endColumn"));
    if (startLine <= 0 || startColumn <= 0 || endLine <= 0 || endColumn <= 0) {
        return out;
    }

    const QJsonValue newTextValue = edit.value(QStringLiteral("newText"));
    if (!newTextValue.isString()) {
        return out;
    }

    QString newText = normalizeNewlines(newTextValue.toString());
    if (newText.size() > maxNewTextChars) {
        return out;
    }

    if (newText.isEmpty() && !options.allowDeletion) {
        return out;
    }

    const KTextEditor::Range range(startLine - 1, startColumn - 1, endLine - 1, endColumn - 1);
    if (!rangeInDocument(document, range)) {
        return out;
    }

    if (options.expectedRange.isValid() && range != options.expectedRange) {
        return out;
    }

    const QString current = normalizeNewlines(document->text(range));
    if (current == newText) {
        return out;
    }

    out.edits = {ProposedEdit{range, newText}};
    out.displayText = displayTextFor(newText);
    out.valid = true;
    return out;
}

} // namespace KateAiInlineCompletion
