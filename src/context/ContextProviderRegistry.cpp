/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: ContextProviderRegistry
*/

#include "context/ContextProviderRegistry.h"

#include <algorithm>
#include <utility>

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

    QVector<ContextItem> out;
    out.reserve(qMin(maxItems, ranked.size()));
    for (const RankedItem &entry : std::as_const(ranked)) {
        if (out.size() >= maxItems) {
            break;
        }
        out.push_back(entry.item);
    }

    return out;
}

} // namespace KateAiInlineCompletion
