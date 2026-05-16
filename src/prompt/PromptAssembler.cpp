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

[[nodiscard]] bool hasSnippetContent(const ContextItem &item)
{
    return item.kind == ContextItem::Kind::CodeSnippet && !hasRecentEditContent(item) && !item.value.trimmed().isEmpty();
}

[[nodiscard]] bool tryAppend(QString *out, const QString &block, int budget)
{
    if (!out) {
        return false;
    }

    if (block.isEmpty()) {
        return true;
    }

    if (budget >= 0 && out->size() + block.size() > budget) {
        return false;
    }

    out->append(block);
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

    QVector<ContextItem> candidates;
    candidates.reserve(items.size());
    for (ContextItem item : items) {
        item.importance = qBound(0, item.importance, 100);
        if (hasTraitContent(item) || hasRecentEditContent(item) || hasSnippetContent(item)) {
            candidates.push_back(item);
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(), itemLess);

    if (candidates.size() > options.maxContextItems) {
        candidates.resize(options.maxContextItems);
    }

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

        if (!tryAppend(&out, block, options.maxContextChars)) {
            continue;
        }
        traitHeaderWritten = true;
    }

    if (traitHeaderWritten) {
        (void)tryAppend(&out, QStringLiteral("\n"), options.maxContextChars);
    }

    QString recentEditsBlock;
    bool recentEditWritten = false;
    for (const ContextItem &item : std::as_const(candidates)) {
        if (!hasRecentEditContent(item)) {
            continue;
        }

        QString itemBlock;
        if (!recentEditWritten) {
            itemBlock += comment + QStringLiteral(" Recently edited files. Continue the user's current edit pattern.\n");
        }
        itemBlock += renderRecentEditValue(comment, item);

        const QString endLine = comment + QStringLiteral(" End of recent edits\n\n");
        if (options.maxContextChars >= 0 && out.size() + recentEditsBlock.size() + itemBlock.size() + endLine.size() > options.maxContextChars) {
            continue;
        }

        recentEditsBlock += itemBlock;
        recentEditWritten = true;
    }

    if (recentEditWritten) {
        recentEditsBlock += comment + QStringLiteral(" End of recent edits\n\n");
        out += recentEditsBlock;
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

        (void)tryAppend(&out, block, options.maxContextChars);
    }

    return out;
}

} // namespace KateAiInlineCompletion
