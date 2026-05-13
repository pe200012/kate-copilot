/*
    SPDX-FileCopyrightText: 2026 kate-ai-inline-completion contributors
    SPDX-License-Identifier: LGPL-2.0-or-later

    Module: ContextProvider

    Synchronous interface for bounded prompt context providers.
*/

#pragma once

#include "context/ContextItem.h"

#include <QVector>

namespace KateAiInlineCompletion
{

class ContextProvider
{
public:
    virtual ~ContextProvider() = default;

    [[nodiscard]] virtual QString id() const = 0;
    [[nodiscard]] virtual int matchScore(const ContextResolveRequest &request) const = 0;
    [[nodiscard]] virtual QVector<ContextItem> resolve(const ContextResolveRequest &request) = 0;
};

} // namespace KateAiInlineCompletion
