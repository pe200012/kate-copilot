/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: CurrentFileContextProvider

    Emits traits describing the current editor location.
*/

#pragma once

#include "context/ContextProvider.h"

namespace KateAiInlineCompletion
{

class CurrentFileContextProvider final : public ContextProvider
{
public:
    [[nodiscard]] QString id() const override;
    [[nodiscard]] int matchScore(const ContextResolveRequest &request) const override;
    [[nodiscard]] QVector<ContextItem> resolve(const ContextResolveRequest &request) override;
};

} // namespace KateAiInlineCompletion
