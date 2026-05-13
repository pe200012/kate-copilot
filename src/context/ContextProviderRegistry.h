/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: ContextProviderRegistry

    Owns context providers and resolves deterministic ranked context items.
*/

#pragma once

#include "context/ContextProvider.h"

#include <memory>
#include <vector>

namespace KateAiInlineCompletion
{

class ContextProviderRegistry final
{
public:
    void addProvider(std::unique_ptr<ContextProvider> provider);

    [[nodiscard]] int providerCount() const;
    [[nodiscard]] QVector<ContextItem> resolve(const ContextResolveRequest &request, int maxItems);

private:
    std::vector<std::unique_ptr<ContextProvider>> m_providers;
};

} // namespace KateAiInlineCompletion
