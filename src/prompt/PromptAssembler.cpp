/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: PromptAssembler
*/

#include "prompt/PromptAssembler.h"

#include <algorithm>
#include <utility>

#include <QFileInfo>
#include <QUrl>
#include <QtGlobal>

namespace KateAiInlineCompletion
{

namespace
{
[[nodiscard]] QString commentPrefixForLanguage(const QString &language)
{
    const QString l = language.toLower();

    if (l.contains(QStringLiteral("python")) || l.contains(QStringLiteral("shell")) || l.contains(QStringLiteral("bash"))
        || l.contains(QStringLiteral("ruby")) || l.contains(QStringLiteral("perl")) || l.contains(QStringLiteral("yaml"))
        || l.contains(QStringLiteral("toml")) || l.contains(QStringLiteral("make")) || l.contains(QStringLiteral("docker"))
        || l.contains(QStringLiteral("cmake"))) {
        return QStringLiteral("#");
    }

    if (l.contains(QStringLiteral("sql"))) {
        return QStringLiteral("--");
    }

    return QStringLiteral("//");
}

[[nodiscard]] int kindOrder(ContextItem::Kind kind)
{
    switch (kind) {
    case ContextItem::Kind::Trait:
        return 0;
    case ContextItem::Kind::CodeSnippet:
        return 1;
    case ContextItem::Kind::DiagnosticBag:
        return 2;
    }

    return 3;
}

[[nodiscard]] bool itemLess(const ContextItem &a, const ContextItem &b)
{
    if (a.importance != b.importance) {
        return a.importance > b.importance;
    }

    const int ak = kindOrder(a.kind);
    const int bk = kindOrder(b.kind);
    if (ak != bk) {
        return ak < bk;
    }

    if (a.providerId != b.providerId) {
        return a.providerId < b.providerId;
    }

    return a.id < b.id;
}

[[nodiscard]] QString displayPathForItem(const ContextItem &item)
{
    if (!item.name.trimmed().isEmpty()) {
        return item.name.trimmed();
    }

    if (!item.uri.trimmed().isEmpty()) {
        const QUrl url(item.uri.trimmed());
        if (url.isValid() && url.isLocalFile()) {
            return QFileInfo(url.toLocalFile()).fileName();
        }
        return item.uri.trimmed();
    }

    return item.id.trimmed();
}

[[nodiscard]] bool hasTraitContent(const ContextItem &item)
{
    return item.kind == ContextItem::Kind::Trait && !item.name.trimmed().isEmpty() && !item.value.trimmed().isEmpty();
}

[[nodiscard]] bool hasRecentEditContent(const ContextItem &item)
{
    return item.kind == ContextItem::Kind::CodeSnippet && item.providerId == QStringLiteral("recent-edits") && !item.value.trimmed().isEmpty();
}

[[nodiscard]] bool hasDiagnosticContent(const ContextItem &item)
{
    return item.kind == ContextItem::Kind::DiagnosticBag && item.providerId == QStringLiteral("diagnostics") && !item.value.trimmed().isEmpty();
}

[[nodiscard]] bool hasRelatedFileContent(const ContextItem &item)
{
    return item.kind == ContextItem::Kind::CodeSnippet && item.providerId == QStringLiteral("related-files") && !item.value.trimmed().isEmpty();
}

[[nodiscard]] bool hasSnippetContent(const ContextItem &item)
{
    return item.kind == ContextItem::Kind::CodeSnippet && !hasRecentEditContent(item) && !hasRelatedFileContent(item) && !item.value.trimmed().isEmpty();
}

[[nodiscard]] bool appendTruncatedBlock(QString *out,
                                        const QString &block,
                                        int budget,
                                        int minUsefulChars,
                                        const QString &ellipsis = QStringLiteral("\n...\n"))
{
    if (!out) {
        return false;
    }

    if (block.isEmpty()) {
        return true;
    }

    if (budget < 0 || out->size() + block.size() <= budget) {
        out->append(block);
        return true;
    }

    const int remaining = budget - out->size();
    const int usefulChars = qMax(0, minUsefulChars);
    if (remaining <= ellipsis.size() || remaining - ellipsis.size() < usefulChars) {
        return false;
    }

    out->append(block.left(remaining - ellipsis.size()));
    out->append(ellipsis);
    return true;
}

[[nodiscard]] QString normalizedSnippet(QString value)
{
    value.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    value.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    if (!value.endsWith(QLatin1Char('\n'))) {
        value += QLatin1Char('\n');
    }
    return value;
}

[[nodiscard]] QString renderRecentEditValue(const QString &comment, const ContextItem &item)
{
    QString block;
    const QStringList lines = normalizedSnippet(item.value).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.isEmpty()) {
            continue;
        }

        if (line.startsWith(QStringLiteral("File:"))) {
            block += comment + QLatin1Char(' ') + line + QLatin1Char('\n');
        } else {
            block += line + QLatin1Char('\n');
        }
    }
    return block;
}

[[nodiscard]] QString renderDiagnosticValue(const QString &comment, const ContextItem &item)
{
    QString block;
    const QStringList lines = normalizedSnippet(item.value).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }
        block += comment + QLatin1Char(' ') + line + QLatin1Char('\n');
    }
    return block;
}

[[nodiscard]] QVector<ContextItem> orderedRenderableCandidates(const QVector<ContextItem> &items, const PromptAssemblyOptions &options)
{
    QVector<ContextItem> candidates;
    candidates.reserve(items.size());
    for (ContextItem item : items) {
        item.importance = qBound(0, item.importance, 100);
        if (hasTraitContent(item) || hasRecentEditContent(item) || hasDiagnosticContent(item) || hasRelatedFileContent(item) || hasSnippetContent(item)) {
            candidates.push_back(item);
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(), itemLess);

    if (candidates.size() > options.maxContextItems) {
        candidates.resize(options.maxContextItems);
    }

    return candidates;
}

[[nodiscard]] QString commentizedLines(const QString &comment, QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    if (text.endsWith(QLatin1Char('\n'))) {
        text.chop(1);
    }

    QString out;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.isEmpty()) {
            out += comment + QLatin1Char('\n');
        } else {
            out += comment + QLatin1Char(' ') + line + QLatin1Char('\n');
        }
    }
    return out;
}

[[nodiscard]] QString copilotContextTextForItem(const ContextItem &item)
{
    const QString path = displayPathForItem(item);
    if (hasTraitContent(item)) {
        return QStringLiteral("Info: %1: %2\n").arg(item.name.trimmed(), item.value.trimmed());
    }
    if (hasDiagnosticContent(item)) {
        return QStringLiteral("Diagnostics: %1\n%2").arg(path, normalizedSnippet(item.value));
    }
    if (hasRecentEditContent(item)) {
        return QStringLiteral("Recent edit: %1\n%2").arg(path, normalizedSnippet(item.value));
    }

    return QStringLiteral("File: %1\n%2").arg(path, normalizedSnippet(item.value));
}
} // namespace

BuiltPrompt PromptAssembler::build(const QString &templateId,
                                   const PromptContext &ctx,
                                   const QVector<ContextItem> &items,
                                   const PromptAssemblyOptions &options)
{
    BuiltPrompt out = PromptTemplate::build(templateId, ctx);

    const QString prefix = renderContextPrefix(ctx, items, options);
    if (!prefix.isEmpty()) {
        out.userPrompt = prefix + out.userPrompt;
    }

    return out;
}

QString PromptAssembler::renderContextPrefix(const PromptContext &ctx, const QVector<ContextItem> &items, const PromptAssemblyOptions &options)
{
    if (!options.enabled || options.maxContextItems <= 0 || options.maxContextChars <= 0 || items.isEmpty()) {
        return {};
    }

    const QVector<ContextItem> candidates = orderedRenderableCandidates(items, options);

    const QString comment = commentPrefixForLanguage(ctx.language);
    QString out;

    bool traitHeaderWritten = false;
    for (const ContextItem &item : std::as_const(candidates)) {
        if (!hasTraitContent(item)) {
            continue;
        }

        QString block;
        if (!traitHeaderWritten) {
            block += comment + QStringLiteral(" Related project information:\n");
        }
        block += comment + QLatin1Char(' ') + item.name.trimmed() + QStringLiteral(": ") + item.value.trimmed() + QLatin1Char('\n');

        if (!appendTruncatedBlock(&out, block, options.maxContextChars, 40)) {
            continue;
        }
        traitHeaderWritten = true;
    }

    if (traitHeaderWritten) {
        (void)appendTruncatedBlock(&out, QStringLiteral("\n"), options.maxContextChars, 1);
    }

    QString recentEditsBlock;
    bool recentEditWritten = false;
    const QString recentEditsEndLine = comment + QStringLiteral(" End of recent edits\n\n");
    for (const ContextItem &item : std::as_const(candidates)) {
        if (!hasRecentEditContent(item)) {
            continue;
        }

        QString itemBlock;
        if (!recentEditWritten) {
            itemBlock += comment + QStringLiteral(" Recently edited files. Continue the user's current edit pattern.\n");
        }
        itemBlock += renderRecentEditValue(comment, item);

        if (options.maxContextChars >= 0 && out.size() + recentEditsBlock.size() + itemBlock.size() + recentEditsEndLine.size() > options.maxContextChars) {
            const int itemBudget = options.maxContextChars - out.size() - recentEditsBlock.size() - recentEditsEndLine.size();
            QString truncatedItemBlock;
            if (appendTruncatedBlock(&truncatedItemBlock, itemBlock, itemBudget, 120)) {
                recentEditsBlock += truncatedItemBlock;
                recentEditWritten = true;
                break;
            }
            continue;
        }

        recentEditsBlock += itemBlock;
        recentEditWritten = true;
    }

    if (recentEditWritten) {
        out += recentEditsBlock + recentEditsEndLine;
    }

    for (const ContextItem &item : std::as_const(candidates)) {
        if (!hasDiagnosticContent(item)) {
            continue;
        }

        const QString path = displayPathForItem(item);
        QString block;
        block += comment + QStringLiteral(" Consider these diagnostics from ") + path + QStringLiteral(":\n");
        block += renderDiagnosticValue(comment, item);
        block += QLatin1Char('\n');

        (void)appendTruncatedBlock(&out, block, options.maxContextChars, 180);
    }

    for (const ContextItem &item : std::as_const(candidates)) {
        if (!hasRelatedFileContent(item)) {
            continue;
        }

        const QString path = displayPathForItem(item);
        QString block;
        block += comment + QStringLiteral(" Compare this related file from ") + path + QStringLiteral(":\n");
        block += normalizedSnippet(item.value);
        block += QLatin1Char('\n');

        (void)appendTruncatedBlock(&out, block, options.maxContextChars, 300);
    }

    for (const ContextItem &item : std::as_const(candidates)) {
        if (!hasSnippetContent(item)) {
            continue;
        }

        const QString path = displayPathForItem(item);
        QString block;
        block += comment + QStringLiteral(" Compare this snippet from ") + path + QStringLiteral(":\n");
        block += normalizedSnippet(item.value);
        block += QLatin1Char('\n');

        (void)appendTruncatedBlock(&out, block, options.maxContextChars, 300);
    }

    return out;
}

QString PromptAssembler::renderCopilotContextPrefix(const PromptContext &ctx,
                                                    const QVector<ContextItem> &items,
                                                    const PromptAssemblyOptions &options)
{
    if (!options.enabled || options.maxContextItems <= 0 || options.maxContextChars <= 0 || items.isEmpty()) {
        return {};
    }

    const QVector<ContextItem> candidates = orderedRenderableCandidates(items, options);
    if (candidates.isEmpty()) {
        return {};
    }

    const QString comment = commentPrefixForLanguage(ctx.language);
    const QString begin = comment + QStringLiteral(" BEGIN RELATED CONTEXT\n");
    const QString end = comment + QStringLiteral(" END RELATED CONTEXT\n");
    if (begin.size() + end.size() > options.maxContextChars) {
        return {};
    }

    QString body;
    const int bodyBudget = options.maxContextChars - begin.size() - end.size();
    const QString ellipsis = QStringLiteral("\n") + comment + QStringLiteral(" ...\n");
    for (const ContextItem &item : std::as_const(candidates)) {
        const QString block = commentizedLines(comment, copilotContextTextForItem(item));
        (void)appendTruncatedBlock(&body, block, bodyBudget, 120, ellipsis);
    }

    if (body.trimmed().isEmpty()) {
        return {};
    }

    return begin + body + end;
}

} // namespace KateAiInlineCompletion
