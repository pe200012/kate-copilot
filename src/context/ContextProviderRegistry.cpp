/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: ContextProviderRegistry
*/

#include "context/ContextProviderRegistry.h"

#include "context/ProjectContextResolver.h"

#include <algorithm>
#include <utility>

#include <QHash>
#include <QtGlobal>

namespace KateAiInlineCompletion
{

namespace
{
struct RankedItem {
    ContextItem item;
    int providerScore = 0;
    int providerOrder = 0;
    int itemOrder = 0;
};

[[nodiscard]] bool rankedItemLess(const RankedItem &a, const RankedItem &b)
{
    if (a.providerScore != b.providerScore) {
        return a.providerScore > b.providerScore;
    }

    if (a.item.importance != b.item.importance) {
        return a.item.importance > b.item.importance;
    }

    if (a.providerOrder != b.providerOrder) {
        return a.providerOrder < b.providerOrder;
    }

    if (a.item.providerId != b.item.providerId) {
        return a.item.providerId < b.item.providerId;
    }

    if (a.item.id != b.item.id) {
        return a.item.id < b.item.id;
    }

    return a.itemOrder < b.itemOrder;
}

[[nodiscard]] bool isSnippetDedupCandidate(const ContextItem &item)
{
    return item.kind == ContextItem::Kind::CodeSnippet && item.providerId != QStringLiteral("recent-edits") && !item.value.trimmed().isEmpty();
}

[[nodiscard]] QString snippetDedupKey(const ContextItem &item)
{
    if (!isSnippetDedupCandidate(item)) {
        return {};
    }

    const QString fromUri = ProjectContextResolver::canonicalPath(item.uri);
    if (!fromUri.isEmpty()) {
        return fromUri;
    }

    return ProjectContextResolver::canonicalPath(item.id);
}

[[nodiscard]] int snippetProviderPriority(const QString &providerId)
{
    if (providerId == QStringLiteral("related-files")) {
        return 0;
    }
    if (providerId == QStringLiteral("open-tabs")) {
        return 1;
    }
    return 2;
}

[[nodiscard]] bool snippetReplacementBetter(const RankedItem &candidate, const RankedItem &current)
{
    const int candidatePriority = snippetProviderPriority(candidate.item.providerId);
    const int currentPriority = snippetProviderPriority(current.item.providerId);
    if (candidatePriority != currentPriority) {
        return candidatePriority < currentPriority;
    }

    return rankedItemLess(candidate, current);
}

[[nodiscard]] QVector<RankedItem> deduplicatedSnippetItems(const QVector<RankedItem> &ranked)
{
    QVector<RankedItem> out;
    out.reserve(ranked.size());

    QHash<QString, int> indexByPath;
    for (const RankedItem &entry : ranked) {
        const QString key = snippetDedupKey(entry.item);
        if (key.isEmpty()) {
            out.push_back(entry);
            continue;
        }

        const auto existing = indexByPath.constFind(key);
        if (existing == indexByPath.constEnd()) {
            indexByPath.insert(key, out.size());
            out.push_back(entry);
            continue;
        }

        const int index = existing.value();
        if (index >= 0 && index < out.size() && snippetReplacementBetter(entry, out.at(index))) {
            out[index] = entry;
        }
    }

    std::stable_sort(out.begin(), out.end(), rankedItemLess);
    return out;
}
} // namespace

void ContextProviderRegistry::addProvider(std::unique_ptr<ContextProvider> provider)
{
    if (!provider) {
        return;
    }

    m_providers.push_back(std::move(provider));
}

int ContextProviderRegistry::providerCount() const
{
    return static_cast<int>(m_providers.size());
}

QVector<ContextItem> ContextProviderRegistry::resolve(const ContextResolveRequest &request, int maxItems)
{
    if (maxItems <= 0) {
        return {};
    }

    QVector<RankedItem> ranked;

    for (int providerIndex = 0; providerIndex < static_cast<int>(m_providers.size()); ++providerIndex) {
        ContextProvider *provider = m_providers.at(static_cast<std::size_t>(providerIndex)).get();
        if (!provider) {
            continue;
        }

        const int score = provider->matchScore(request);
        if (score <= 0) {
            continue;
        }

        QVector<ContextItem> items = provider->resolve(request);
        for (int itemIndex = 0; itemIndex < items.size(); ++itemIndex) {
            ContextItem item = items.at(itemIndex);
            if (item.providerId.trimmed().isEmpty()) {
                item.providerId = provider->id();
            }
            item.importance = qBound(0, item.importance, 100);

            ranked.push_back(RankedItem{item, score, providerIndex, itemIndex});
        }
    }

    std::stable_sort(ranked.begin(), ranked.end(), rankedItemLess);

    const QVector<RankedItem> deduped = deduplicatedSnippetItems(ranked);

    QVector<ContextItem> out;
    out.reserve(qMin(maxItems, deduped.size()));
    for (const RankedItem &entry : std::as_const(deduped)) {
        if (out.size() >= maxItems) {
            break;
        }
        out.push_back(entry.item);
    }

    return out;
}

} // namespace KateAiInlineCompletion
