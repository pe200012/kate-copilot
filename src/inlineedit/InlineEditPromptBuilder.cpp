/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: InlineEditPromptBuilder
*/

#include "inlineedit/InlineEditPromptBuilder.h"

#include "security/SensitiveDataRedactor.h"

#include <QStringList>
#include <QtGlobal>

#include <utility>

namespace KateAiInlineCompletion
{
namespace
{
[[nodiscard]] QString targetText(const InlineEditRequestContext &context)
{
    if (!context.selectedText.isEmpty()) {
        return context.selectedText;
    }

    return context.currentTargetText;
}

[[nodiscard]] QString bounded(QString text, int maxChars)
{
    if (maxChars <= 0) {
        return {};
    }

    if (text.size() > maxChars) {
        text.truncate(maxChars);
    }
    return text;
}

[[nodiscard]] QString kindName(ContextItem::Kind kind)
{
    switch (kind) {
    case ContextItem::Kind::Trait:
        return QStringLiteral("trait");
    case ContextItem::Kind::CodeSnippet:
        return QStringLiteral("code");
    case ContextItem::Kind::DiagnosticBag:
        return QStringLiteral("diagnostics");
    }
    return QStringLiteral("context");
}

[[nodiscard]] QString renderContextItems(const QVector<ContextItem> &items, const InlineEditPromptOptions &options)
{
    if (!options.useContext || items.isEmpty() || options.maxContextChars <= 0) {
        return QStringLiteral("None");
    }

    QString out;
    int remaining = options.maxContextChars;
    for (const ContextItem &item : items) {
        QString header = QStringLiteral("[%1] provider=%2 name=%3 uri=%4\n")
                             .arg(kindName(item.kind), item.providerId, item.name, item.uri);
        QString body = item.value;
        if (!body.endsWith(QLatin1Char('\n'))) {
            body += QLatin1Char('\n');
        }
        const QString block = header + body;
        if (block.size() <= remaining) {
            out += block;
            remaining -= block.size();
            continue;
        }

        if (remaining > 32) {
            out += bounded(block, remaining - 1);
            out += QLatin1Char('\n');
        }
        break;
    }

    return out.trimmed().isEmpty() ? QStringLiteral("None") : out.trimmed();
}

[[nodiscard]] QString rangeLine(const QString &name, int value)
{
    return QStringLiteral("%1: %2").arg(name, QString::number(value));
}

[[nodiscard]] QString boundedRedacted(QString text, int maxChars)
{
    text = redactSensitiveData(std::move(text)).trimmed();
    if (maxChars <= 0) {
        return {};
    }
    if (text.size() > maxChars) {
        text.truncate(maxChars);
    }
    return text;
}

[[nodiscard]] QString triggerDetailText(const InlineEditRequestContext &context, const InlineEditPromptOptions &options)
{
    const InlineEditTrigger &trigger = options.trigger;
    switch (trigger.kind) {
    case InlineEditTriggerKind::DiagnosticRepair:
        return boundedRedacted(trigger.diagnosticMessage.isEmpty() ? trigger.reason : trigger.diagnosticMessage, options.maxTriggerPromptChars);
    case InlineEditTriggerKind::RecentEditContinuation:
        return boundedRedacted(trigger.recentEditSummary.isEmpty() ? trigger.reason : trigger.recentEditSummary, options.maxTriggerPromptChars);
    case InlineEditTriggerKind::SelectionRepair:
        return boundedRedacted(context.selectedText.isEmpty() ? trigger.reason : context.selectedText, options.maxTriggerPromptChars);
    case InlineEditTriggerKind::Manual:
        return boundedRedacted(trigger.reason, options.maxTriggerPromptChars);
    }
    return {};
}

void appendTriggerSection(QStringList &lines, const InlineEditRequestContext &context, const InlineEditPromptOptions &options)
{
    const InlineEditTrigger &trigger = options.trigger;
    if (trigger.kind == InlineEditTriggerKind::Manual && trigger.reason.trimmed().isEmpty()) {
        return;
    }

    lines << QString();
    lines << QStringLiteral("Trigger reason: %1").arg(inlineEditTriggerKindName(trigger.kind));

    switch (trigger.kind) {
    case InlineEditTriggerKind::DiagnosticRepair:
        lines << QStringLiteral("Diagnostic:");
        break;
    case InlineEditTriggerKind::RecentEditContinuation:
        lines << QStringLiteral("Recent edit pattern:");
        break;
    case InlineEditTriggerKind::SelectionRepair:
        lines << QStringLiteral("Selected code:");
        break;
    case InlineEditTriggerKind::Manual:
        lines << QStringLiteral("Manual request:");
        break;
    }

    const QString detail = triggerDetailText(context, options);
    if (!detail.isEmpty()) {
        lines << detail;
    }
}
} // namespace

InlineEditPrompt InlineEditPromptBuilder::build(const InlineEditRequestContext &context, const InlineEditPromptOptions &options)
{
    InlineEditPrompt prompt;
    prompt.systemPrompt = QStringLiteral("You are an inline code edit engine. Return JSON only. Do not return Markdown.");

    const KTextEditor::Range range = context.targetRange;
    QStringList lines;
    lines << QStringLiteral("File: %1").arg(context.filePath);
    lines << QStringLiteral("Language: %1").arg(context.languageId);
    lines << QStringLiteral("Cursor: line %1, column %2").arg(context.cursor.line() + 1).arg(context.cursor.column() + 1);
    lines << QString();
    lines << QStringLiteral("Target range:");
    lines << rangeLine(QStringLiteral("startLine"), range.start().line() + 1);
    lines << rangeLine(QStringLiteral("startColumn"), range.start().column() + 1);
    lines << rangeLine(QStringLiteral("endLine"), range.end().line() + 1);
    lines << rangeLine(QStringLiteral("endColumn"), range.end().column() + 1);
    lines << QString();
    lines << QStringLiteral("Current target text:");
    lines << targetText(context);
    lines << QString();
    lines << QStringLiteral("Nearby prefix excerpt:");
    lines << context.prefixExcerpt;
    lines << QString();
    lines << QStringLiteral("Nearby suffix excerpt:");
    lines << context.suffixExcerpt;
    lines << QString();
    lines << QStringLiteral("Relevant context:");
    lines << renderContextItems(context.contextItems, options);
    appendTriggerSection(lines, context, options);
    lines << QString();
    lines << QStringLiteral("Return 1 to %1 non-overlapping edits inside the target range above.").arg(qMax(1, options.maxEdits));
    lines << QStringLiteral("Use the smallest edit set that solves the request; prefer one edit for local changes.");
    lines << QStringLiteral("Preserve edit order by importance; the client validates and applies ranges transactionally.");
    lines << QStringLiteral("Return:");
    lines << QStringLiteral("{");
    lines << QStringLiteral("  \"edits\": [");
    lines << QStringLiteral("    {");
    lines << QStringLiteral("      \"startLine\": <1-based>,");
    lines << QStringLiteral("      \"startColumn\": <1-based>,");
    lines << QStringLiteral("      \"endLine\": <1-based>,");
    lines << QStringLiteral("      \"endColumn\": <1-based>,");
    lines << QStringLiteral("      \"newText\": \"replacement text\"");
    lines << QStringLiteral("    }");
    lines << QStringLiteral("  ]");
    lines << QStringLiteral("}");

    prompt.userPrompt = lines.join(QLatin1Char('\n'));
    return prompt;
}

} // namespace KateAiInlineCompletion
